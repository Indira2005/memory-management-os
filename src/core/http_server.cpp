#include "http_server.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "allocation_simulator.hpp"
#include "demand_paging_simulator.hpp"
#include "json_utils.hpp"
#include "multi_level_paging_simulator.hpp"
#include "paging_simulator.hpp"
#include "segmentation_simulator.hpp"
#include "thrashing_simulator.hpp"
#include "translation_simulator.hpp"
#include "utils.hpp"
#include "web_assets.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
static constexpr SocketHandle invalid_socket_handle = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle invalid_socket_handle = -1;
#endif

namespace {
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

class SocketRuntime {
public:
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed.");
        }
#endif
    }

    ~SocketRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

void close_socket(SocketHandle socket_handle) {
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

std::string http_response(
    int status_code,
    const std::string& status_text,
    const std::string& content_type,
    const std::string& body
) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Access-Control-Allow-Origin: *\r\n\r\n"
             << body;
    return response.str();
}

bool send_all(SocketHandle socket_handle, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int chunk = send(
            socket_handle,
            data.data() + sent,
            static_cast<int>(data.size() - sent),
            0
        );
        if (chunk <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(chunk);
    }
    return true;
}

HttpRequest parse_request(const std::string& raw) {
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("Malformed HTTP request.");
    }

    HttpRequest request;
    std::istringstream stream(raw.substr(0, header_end));
    std::string request_line;
    std::getline(stream, request_line);
    request_line = trim(request_line);

    {
        std::istringstream line_stream(request_line);
        line_stream >> request.method >> request.path;
    }

    std::string header_line;
    while (std::getline(stream, header_line)) {
        header_line = trim(header_line);
        if (header_line.empty()) {
            continue;
        }
        const auto separator = header_line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        request.headers[to_lower(trim(header_line.substr(0, separator)))] = trim(header_line.substr(separator + 1));
    }

    request.body = raw.substr(header_end + 4);
    return request;
}

std::string read_request(SocketHandle client_socket) {
    std::string request;
    char buffer[4096];
    std::size_t content_length = 0;
    bool has_headers = false;

    while (true) {
        const int received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }

        request.append(buffer, buffer + received);

        if (!has_headers) {
            const auto header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                has_headers = true;
                const std::string header_part = request.substr(0, header_end);
                const auto lowered = to_lower(header_part);
                const auto content_pos = lowered.find("content-length:");
                if (content_pos != std::string::npos) {
                    const auto line_end = header_part.find("\r\n", content_pos);
                    const std::string length_value = trim(header_part.substr(
                        content_pos + 15,
                        line_end == std::string::npos ? std::string::npos : line_end - (content_pos + 15)
                    ));
                    content_length = static_cast<std::size_t>(std::stoi(length_value));
                }
            }
        }

        if (has_headers) {
            const auto header_end = request.find("\r\n\r\n");
            const std::size_t body_size = request.size() - (header_end + 4);
            if (body_size >= content_length) {
                break;
            }
        }
    }

    return request;
}

std::map<std::string, std::string> parse_form_encoded(const std::string& body) {
    std::map<std::string, std::string> fields;
    for (const auto& pair : split_any(body, "&")) {
        const auto separator = pair.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        fields[url_decode(pair.substr(0, separator))] = url_decode(pair.substr(separator + 1));
    }
    return fields;
}

AllocationAlgorithm parse_allocation_algorithm(const std::string& value) {
    const std::string normalized = to_lower(value);
    if (normalized == "first_fit") return AllocationAlgorithm::FirstFit;
    if (normalized == "best_fit") return AllocationAlgorithm::BestFit;
    if (normalized == "worst_fit") return AllocationAlgorithm::WorstFit;
    if (normalized == "next_fit") return AllocationAlgorithm::NextFit;
    throw std::runtime_error("Unknown allocation algorithm.");
}

PagingAlgorithm parse_paging_algorithm(const std::string& value) {
    const std::string normalized = to_lower(value);
    if (normalized == "fifo") return PagingAlgorithm::FIFO;
    if (normalized == "lru") return PagingAlgorithm::LRU;
    if (normalized == "optimal") return PagingAlgorithm::Optimal;
    if (normalized == "second_chance") return PagingAlgorithm::SecondChance;
    throw std::runtime_error("Unknown paging algorithm.");
}

std::vector<AllocationOperation> parse_operations(const std::string& input) {
    std::vector<AllocationOperation> operations;

    for (const auto& raw_line : split_lines(input)) {
        const std::string line = trim(raw_line);
        if (line.empty()) {
            continue;
        }

        const auto tokens = split_any(line, " \t");
        if (tokens.empty()) {
            continue;
        }

        const std::string command = to_lower(tokens[0]);
        if ((command == "alloc" || command == "allocate") && tokens.size() == 3) {
            operations.push_back({AllocationOperation::Type::Allocate, tokens[1], std::stoi(tokens[2])});
        } else if ((command == "free" || command == "dealloc" || command == "deallocate") && tokens.size() >= 2) {
            operations.push_back({AllocationOperation::Type::Free, tokens[1], 0});
        } else {
            throw std::runtime_error("Invalid allocation line. Use 'ALLOC P1 120' or 'FREE P1'.");
        }
    }

    if (operations.empty()) {
        throw std::runtime_error("At least one allocation operation is required.");
    }

    return operations;
}

std::vector<int> parse_reference_string(const std::string& input) {
    std::vector<int> references;
    for (const auto& token : split_any(input, ", \t\r\n")) {
        if (!token.empty()) {
            references.push_back(std::stoi(token));
        }
    }
    if (references.empty()) {
        throw std::runtime_error("Reference string must contain at least one page number.");
    }
    return references;
}

std::vector<TranslationEntry> parse_page_table(const std::string& input) {
    std::vector<TranslationEntry> page_table;
    std::string normalized = replace_all(input, "\n", ",");
    normalized = replace_all(normalized, "\r", ",");

    for (const auto& token : split_any(normalized, ",")) {
        if (token.empty()) {
            continue;
        }

        std::string entry = trim(replace_all(token, "->", ":"));
        const auto colon = entry.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error("Invalid page-table entry. Use values like '0:5' or '2:-'.");
        }

        const int page = std::stoi(trim(entry.substr(0, colon)));
        const std::string frame_text = trim(entry.substr(colon + 1));
        if (frame_text == "-" || to_lower(frame_text) == "x" || to_lower(frame_text) == "none") {
            page_table.push_back({page, -1, false});
        } else {
            page_table.push_back({page, std::stoi(frame_text), true});
        }
    }

    if (page_table.empty()) {
        throw std::runtime_error("Page table must contain at least one entry.");
    }

    std::sort(
        page_table.begin(),
        page_table.end(),
        [](const TranslationEntry& left, const TranslationEntry& right) { return left.page < right.page; }
    );
    return page_table;
}

std::vector<TlbEntry> parse_tlb(const std::string& input) {
    std::vector<TlbEntry> tlb;
    if (trim(input).empty()) {
        return tlb;
    }

    std::string normalized = replace_all(input, "\n", ",");
    normalized = replace_all(normalized, "\r", ",");
    for (const auto& token : split_any(normalized, ",")) {
        if (token.empty()) {
            continue;
        }
        const auto colon = token.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error("Invalid TLB entry. Use values like '2:8'.");
        }
        tlb.push_back({std::stoi(trim(token.substr(0, colon))), std::stoi(trim(token.substr(colon + 1)))});
    }
    return tlb;
}

std::vector<SegmentEntry> parse_segment_table(const std::string& input) {
    std::vector<SegmentEntry> table;
    std::string normalized = replace_all(input, "\n", ",");
    normalized = replace_all(normalized, "\r", ",");
    for (const auto& token : split_any(normalized, ",")) {
        if (token.empty()) {
            continue;
        }
        const auto parts = split_any(token, ":");
        if (parts.size() != 3) {
            throw std::runtime_error("Invalid segment entry. Use values like '1:400:120'.");
        }
        table.push_back({std::stoi(parts[0]), std::stoi(parts[1]), std::stoi(parts[2])});
    }
    return table;
}

std::vector<MultiLevelEntry> parse_multi_level_map(const std::string& input) {
    std::vector<MultiLevelEntry> map;
    std::string normalized = replace_all(input, "\n", ",");
    normalized = replace_all(normalized, "\r", ",");
    for (const auto& token : split_any(normalized, ",")) {
        if (token.empty()) {
            continue;
        }
        const auto parts = split_any(token, ":");
        if (parts.size() != 3) {
            throw std::runtime_error("Invalid two-level entry. Use values like '1:3:8' or '1:3:-'.");
        }
        const int outer_index = std::stoi(parts[0]);
        const int inner_index = std::stoi(parts[1]);
        const std::string frame_text = trim(parts[2]);
        if (frame_text == "-" || to_lower(frame_text) == "none") {
            map.push_back({outer_index, inner_index, -1, false});
        } else {
            map.push_back({outer_index, inner_index, std::stoi(frame_text), true});
        }
    }
    return map;
}

std::string handle_request(const HttpRequest& request) {
    if (request.method == "GET" && request.path == "/") {
        return http_response(200, "OK", "text/html; charset=utf-8", index_html());
    }
    if (request.method == "GET" && request.path == "/styles.css") {
        return http_response(200, "OK", "text/css; charset=utf-8", stylesheet());
    }
    if (request.method == "GET" && request.path == "/app.js") {
        return http_response(200, "OK", "application/javascript; charset=utf-8", app_script());
    }

    if (request.method != "POST") {
        return http_response(404, "Not Found", "text/plain; charset=utf-8", "Route not found.");
    }

    try {
        const auto fields = parse_form_encoded(request.body);

        if (request.path == "/simulate/allocation") {
            AllocationSimulator simulator;
            const auto result = simulator.simulate(
                parse_allocation_algorithm(fields.at("algorithm")),
                std::stoi(fields.at("total_memory")),
                parse_operations(fields.at("operations"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/paging") {
            PagingSimulator simulator;
            const auto result = simulator.simulate(
                parse_paging_algorithm(fields.at("algorithm")),
                std::stoi(fields.at("frame_count")),
                parse_reference_string(fields.at("reference_string"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/translation") {
            TranslationSimulator simulator;
            const auto result = simulator.simulate(
                std::stoi(fields.at("page_size")),
                std::stoi(fields.at("logical_address")),
                parse_page_table(fields.at("page_table"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/tlb") {
            TranslationSimulator simulator;
            const auto result = simulator.simulate_with_tlb(
                std::stoi(fields.at("page_size")),
                std::stoi(fields.at("logical_address")),
                parse_page_table(fields.at("page_table")),
                parse_tlb(fields.at("tlb"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/demand-paging") {
            DemandPagingSimulator simulator;
            const auto result = simulator.simulate(
                std::stoi(fields.at("frame_count")),
                std::stoi(fields.at("tlb_size")),
                parse_reference_string(fields.at("reference_string"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/segmentation") {
            SegmentationSimulator simulator;
            const auto result = simulator.simulate(
                std::stoi(fields.at("logical_address")),
                std::stoi(fields.at("bits_for_offset")),
                parse_segment_table(fields.at("segment_table"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/multi-level-paging") {
            MultiLevelPagingSimulator simulator;
            const auto result = simulator.simulate(
                std::stoi(fields.at("page_size")),
                std::stoi(fields.at("inner_bits")),
                std::stoi(fields.at("logical_address")),
                parse_multi_level_map(fields.at("page_map"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }

        if (request.path == "/simulate/thrashing") {
            ThrashingSimulator simulator;
            const auto result = simulator.simulate(
                parse_paging_algorithm(fields.at("algorithm")),
                std::stoi(fields.at("min_frames")),
                std::stoi(fields.at("max_frames")),
                std::stoi(fields.at("threshold")),
                parse_reference_string(fields.at("reference_string"))
            );
            return http_response(200, "OK", "application/json; charset=utf-8", to_json(result));
        }
    } catch (const std::exception& ex) {
        return http_response(400, "Bad Request", "text/plain; charset=utf-8", ex.what());
    }

    return http_response(404, "Not Found", "text/plain; charset=utf-8", "Route not found.");
}
}  // namespace

HttpServer::HttpServer(int port) : port_(port) {}

void HttpServer::run() {
    SocketRuntime runtime;

    const SocketHandle server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == invalid_socket_handle) {
        throw std::runtime_error("Unable to create socket.");
    }

    const int reuse = 1;
    setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
#ifdef _WIN32
        reinterpret_cast<const char*>(&reuse),
#else
        &reuse,
#endif
        sizeof(reuse)
    );

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port_));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close_socket(server_socket);
        throw std::runtime_error("Unable to bind server socket.");
    }

    if (listen(server_socket, 10) < 0) {
        close_socket(server_socket);
        throw std::runtime_error("Unable to listen on server socket.");
    }

    std::cout << "OS Memory Management Simulator is running.\n";
    std::cout << "Open http://127.0.0.1:" << port_ << " in your browser.\n";
    std::cout << "Press Ctrl+C to stop the server.\n";

    while (true) {
        sockaddr_in client_address {};
#ifdef _WIN32
        int client_length = sizeof(client_address);
#else
        socklen_t client_length = sizeof(client_address);
#endif
        const SocketHandle client_socket = accept(
            server_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_length
        );

        if (client_socket == invalid_socket_handle) {
            continue;
        }

        const std::string raw_request = read_request(client_socket);
        if (!raw_request.empty()) {
            try {
                const HttpRequest request = parse_request(raw_request);
                const std::string response = handle_request(request);
                send_all(client_socket, response);
            } catch (const std::exception& ex) {
                send_all(
                    client_socket,
                    http_response(400, "Bad Request", "text/plain; charset=utf-8", ex.what())
                );
            }
        }

        close_socket(client_socket);
    }
}
