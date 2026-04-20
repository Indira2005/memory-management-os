const allocationColors = ['#f97316', '#0ea5e9', '#ef4444', '#22c55e', '#8b5cf6', '#14b8a6', '#f43f5e', '#eab308'];
const allocationAlgorithms = ['first_fit', 'best_fit', 'worst_fit', 'next_fit'];
const pagingAlgorithms = ['fifo', 'lru', 'optimal', 'second_chance'];
let playerIdCounter = 0;
const playerStore = new Map();

const allocationPresets = {
  fragmentation_demo: {
    totalMemory: 320,
    operations: `ALLOC P1 120
ALLOC P2 80
ALLOC P3 60
FREE P2
ALLOC P4 50
ALLOC P5 30`,
  },
  balanced_mix: {
    totalMemory: 360,
    operations: `ALLOC P1 90
ALLOC P2 70
ALLOC P3 50
FREE P2
ALLOC P4 40
ALLOC P5 60
FREE P1
ALLOC P6 80`,
  },
  large_process_pressure: {
    totalMemory: 400,
    operations: `ALLOC P1 140
ALLOC P2 90
ALLOC P3 60
FREE P2
ALLOC P4 110
ALLOC P5 70`,
  },
};

const pagingPresets = {
  classic_string: {
    frameCount: 3,
    referenceString: `7 0 1 2 0 3 0 4 2 3 0 3 2`,
  },
  locality_demo: {
    frameCount: 4,
    referenceString: `1 2 3 2 1 4 1 2 5 1 2 3 2 1`,
  },
  fault_heavy: {
    frameCount: 3,
    referenceString: `1 2 3 4 1 2 5 1 2 3 4 5`,
  },
};

const translationPresets = {
  fault_case: {
    pageSize: 64,
    logicalAddress: 130,
    pageTable: `0:5,1:2,2:-,3:7`,
  },
  successful_lookup: {
    pageSize: 64,
    logicalAddress: 75,
    pageTable: `0:4,1:1,2:6,3:7`,
  },
  higher_page: {
    pageSize: 128,
    logicalAddress: 530,
    pageTable: `0:2,1:5,2:8,3:-,4:11`,
  },
};

const tabs = [...document.querySelectorAll('.tab')];
const modules = [...document.querySelectorAll('.module')];

tabs.forEach((tab) => {
  tab.addEventListener('click', () => {
    tabs.forEach((item) => item.classList.remove('active'));
    modules.forEach((item) => item.classList.remove('active'));
    tab.classList.add('active');
    document.getElementById(tab.dataset.target).classList.add('active');
  });
});

function colorForOwner(owner) {
  if (owner === 'FREE') {
    return '';
  }
  let hash = 0;
  for (const ch of owner) {
    hash += ch.charCodeAt(0);
  }
  return allocationColors[Math.abs(hash) % allocationColors.length];
}

function statCard(label, value) {
  return `<div class="stat"><span>${label}</span><strong>${value}</strong></div>`;
}

function renderLegend() {
  const freeColor = getComputedStyle(document.documentElement).getPropertyValue('--free').trim();
  return `
    <div class="legend">
      <span class="legend-item"><span class="swatch" style="background:${freeColor}"></span>Free block</span>
      <span class="legend-item"><span class="swatch" style="background:${allocationColors[0]}"></span>Allocated process</span>
    </div>
  `;
}

function renderMemoryBar(blocks) {
  return `
    <div class="memory-bar animated-bar">
      ${blocks.map((block) => `
        <div
          class="memory-segment ${block.free ? 'free' : ''}"
          style="flex:${Math.max(block.size, 1)}; background:${block.free ? '' : colorForOwner(block.owner)}"
          title="${block.owner} | ${block.start}-${block.end} | ${block.size} KB">
          <div>
            <div>${block.owner}</div>
            <small>${block.size} KB</small>
          </div>
        </div>
      `).join('')}
    </div>
  `;
}

function getAllocationChange(step, previousStep) {
  if (!previousStep) {
    return { type: 'initial', owner: null, start: null, message: 'Simulation starts with all memory free.' };
  }

  const previousOwners = new Set(previousStep.blocks.filter((block) => !block.free).map((block) => block.owner));
  const currentOwners = new Set(step.blocks.filter((block) => !block.free).map((block) => block.owner));

  for (const owner of currentOwners) {
    if (!previousOwners.has(owner)) {
      const block = step.blocks.find((item) => item.owner === owner);
      return {
        type: 'allocated',
        owner,
        start: block ? block.start : null,
        message: `${owner} was placed into memory in this step.`,
      };
    }
  }

  for (const owner of previousOwners) {
    if (!currentOwners.has(owner)) {
      return {
        type: 'freed',
        owner,
        start: null,
        message: `${owner} was removed and its space became free in this step.`,
      };
    }
  }

  return {
    type: step.status,
    owner: null,
    start: null,
    message: step.details,
  };
}

function getPagingChange(step) {
  if (step.hit) {
    return {
      type: 'hit',
      frameIndex: step.frames.findIndex((value) => value === step.page),
      message: `Page ${step.page} was already present, so no replacement was needed.`,
    };
  }

  const frameIndex = step.frames.findIndex((value) => value === step.page);
  if (step.replacedPage === -1) {
    return {
      type: 'loaded',
      frameIndex,
      message: `Page ${step.page} entered an empty frame.`,
    };
  }

  return {
    type: 'replaced',
    frameIndex,
    message: `Page ${step.replacedPage} was replaced by page ${step.page}.`,
  };
}

function renderEventBanner(change) {
  const labelMap = {
    initial: 'Initial State',
    allocated: 'Allocated',
    freed: 'Freed',
    success: 'Success',
    error: 'Error',
    warning: 'Warning',
    hit: 'Page Hit',
    loaded: 'Page Loaded',
    replaced: 'Page Replaced',
  };

  const tone = ['error'].includes(change.type)
    ? 'error'
    : ['warning', 'replaced'].includes(change.type)
      ? 'warning'
      : 'success';

  return `
    <div class="event-banner ${tone}">
      <span class="event-pill">${labelMap[change.type] || 'Event'}</span>
      <strong>${change.message}</strong>
    </div>
  `;
}

function allocationObservation(result) {
  if (result.failedAllocations > 0 && result.finalMetrics.totalFreeMemory > 0) {
    return 'At least one request failed even though free memory remained. That is a strong fragmentation example for your demo.';
  }
  if (result.algorithm === 'Best Fit') {
    return 'Best Fit tries to reduce immediate waste by choosing the smallest possible hole, but it can leave many tiny gaps behind.';
  }
  if (result.algorithm === 'Worst Fit') {
    return 'Worst Fit always breaks the largest hole first, which can preserve larger free regions for upcoming requests.';
  }
  if (result.algorithm === 'Next Fit') {
    return 'Next Fit keeps searching from the last placement point, so the allocation pattern spreads through memory instead of always starting at the front.';
  }
  return 'First Fit is easy to explain: the first hole large enough for the process is chosen immediately.';
}

function pagingObservation(result) {
  if (result.algorithm === 'Optimal') {
    return 'Optimal is a benchmark policy. It gives the minimum possible faults because it knows the future reference pattern.';
  }
  if (result.algorithm === 'LRU') {
    return 'LRU uses recent history as a predictor: if a page has not been used for a long time, it is considered a better victim.';
  }
  if (result.algorithm === 'Second Chance') {
    return 'Second Chance improves FIFO by checking whether a page was referenced recently before replacing it.';
  }
  return 'FIFO is simple to implement, but it does not consider actual usage patterns, so it can replace pages that are still useful.';
}

function createPlayer(type, result) {
  const id = `player-${++playerIdCounter}`;
  playerStore.set(id, {
    type,
    result,
    stepIndex: 0,
    timer: null,
  });
  return id;
}

function renderPlayerShell(id, title, subtitle) {
  return `
    <div class="demo-player" data-player-id="${id}">
      <div class="player-top">
        <div>
          <strong>${title}</strong>
          <div class="muted">${subtitle}</div>
        </div>
        <div class="player-badge">Live Demo View</div>
      </div>
      <div class="player-controls">
        <button class="ghost player-prev" type="button">Prev</button>
        <button class="primary player-toggle" type="button">Play</button>
        <button class="ghost player-next" type="button">Next</button>
        <label class="player-range-wrap">
          <span>Step</span>
          <input class="player-range" type="range" min="0" value="0">
        </label>
        <div class="player-step-label"></div>
      </div>
      <div class="player-stage"></div>
    </div>
  `;
}

function renderAllocationStage(result, stepIndex) {
  const step = result.steps[stepIndex];
  const previousStep = stepIndex > 0 ? result.steps[stepIndex - 1] : null;
  const change = getAllocationChange(step, previousStep);
  return `
    <article class="step current-step">
      <div class="step-header">
        <div>
          <strong>Step ${stepIndex}</strong>
          <div>${step.title}</div>
        </div>
        <span class="chip ${step.status}">${step.status}</span>
      </div>
      <p class="muted">${step.details}</p>
      ${renderEventBanner(change)}
      <div class="progress-line">
        <div class="progress-fill" style="width:${((stepIndex + 1) / result.steps.length) * 100}%"></div>
      </div>
      <div class="process-strip">
        ${step.blocks.filter((block) => !block.free).map((block) => `
          <div class="process-card ${change.owner === block.owner ? 'process-card-active' : ''}">
            <span>${block.owner}</span>
            <strong>${block.size} KB</strong>
          </div>
        `).join('') || '<div class="muted">No active processes in memory.</div>'}
      </div>
      <div class="memory-bar animated-bar">
        ${step.blocks.map((block) => {
          const isChanged =
            (!block.free && change.owner && block.owner === change.owner) ||
            (block.free && change.type === 'freed');
          return `
            <div
              class="memory-segment ${block.free ? 'free' : ''} ${isChanged ? 'memory-segment-focus' : ''}"
              style="flex:${Math.max(block.size, 1)}; background:${block.free ? '' : colorForOwner(block.owner)}"
              title="${block.owner} | ${block.start}-${block.end} | ${block.size} KB">
              <div>
                <div>${block.owner}</div>
                <small>${block.size} KB</small>
              </div>
            </div>
          `;
        }).join('')}
      </div>
      ${renderLegend()}
      <div class="mini-grid">
        <div class="mini-card"><span class="muted">Used memory</span><strong>${step.metrics.usedMemory} KB</strong></div>
        <div class="mini-card"><span class="muted">Free memory</span><strong>${step.metrics.totalFreeMemory} KB</strong></div>
        <div class="mini-card"><span class="muted">Largest hole</span><strong>${step.metrics.largestHole} KB</strong></div>
        <div class="mini-card"><span class="muted">External fragmentation</span><strong>${step.metrics.externalFragmentation} KB</strong></div>
      </div>
    </article>
  `;
}

function renderPagingStage(result, stepIndex) {
  const step = result.steps[stepIndex];
  const change = getPagingChange(step);
  return `
    <article class="step current-step">
      <div class="step-header">
        <div>
          <strong>Reference ${stepIndex + 1}</strong>
          <div>Page ${step.page}</div>
        </div>
        <span class="chip ${step.hit ? 'success' : 'warning'}">${step.hit ? 'hit' : 'page fault'}</span>
      </div>
      <p class="muted">${step.details}</p>
      ${renderEventBanner(change)}
      <div class="progress-line">
        <div class="progress-fill" style="width:${((stepIndex + 1) / result.steps.length) * 100}%"></div>
      </div>
      <div class="reference-track">
        ${result.referenceString.map((page, index) => `
          <div class="reference-chip ${index === stepIndex ? 'reference-chip-active' : ''} ${index < stepIndex ? 'reference-chip-done' : ''}">
            ${page}
          </div>
        `).join('')}
      </div>
      <div class="frame-row">
        ${step.frames.map((frame, frameIndex) => `
          <div class="frame animated-frame ${frame === -1 ? 'frame-empty' : ''} ${change.frameIndex === frameIndex ? 'frame-focus' : ''}">
            <span class="muted">Frame ${frameIndex}</span>
            <strong>${frame === -1 ? '-' : frame}</strong>
            ${step.referenceBits.length ? `<small class="muted">Ref bit: ${step.referenceBits[frameIndex]}</small>` : ''}
          </div>
        `).join('')}
      </div>
    </article>
  `;
}

function updatePlayer(id) {
  const state = playerStore.get(id);
  const root = document.querySelector(`[data-player-id="${id}"]`);
  if (!state || !root) return;

  const maxIndex = state.result.steps.length - 1;
  const stepIndex = Math.max(0, Math.min(state.stepIndex, maxIndex));
  state.stepIndex = stepIndex;

  root.querySelector('.player-range').max = maxIndex;
  root.querySelector('.player-range').value = stepIndex;
  root.querySelector('.player-step-label').textContent = `${stepIndex + 1} / ${state.result.steps.length}`;
  root.querySelector('.player-stage').innerHTML =
    state.type === 'allocation'
      ? renderAllocationStage(state.result, stepIndex)
      : renderPagingStage(state.result, stepIndex);
}

function stopPlayer(id) {
  const state = playerStore.get(id);
  if (!state || !state.timer) return;
  clearInterval(state.timer);
  state.timer = null;
  const root = document.querySelector(`[data-player-id="${id}"]`);
  if (root) {
    root.querySelector('.player-toggle').textContent = 'Play';
  }
}

function startPlayer(id) {
  const state = playerStore.get(id);
  const root = document.querySelector(`[data-player-id="${id}"]`);
  if (!state || !root || state.timer) return;

  root.querySelector('.player-toggle').textContent = 'Pause';
  state.timer = setInterval(() => {
    if (state.stepIndex >= state.result.steps.length - 1) {
      stopPlayer(id);
      return;
    }
    state.stepIndex += 1;
    updatePlayer(id);
  }, 1100);
}

function attachPlayer(root, id) {
  const range = root.querySelector('.player-range');
  root.querySelector('.player-prev').addEventListener('click', () => {
    const state = playerStore.get(id);
    stopPlayer(id);
    state.stepIndex = Math.max(0, state.stepIndex - 1);
    updatePlayer(id);
  });
  root.querySelector('.player-next').addEventListener('click', () => {
    const state = playerStore.get(id);
    stopPlayer(id);
    state.stepIndex = Math.min(state.result.steps.length - 1, state.stepIndex + 1);
    updatePlayer(id);
  });
  root.querySelector('.player-toggle').addEventListener('click', () => {
    const state = playerStore.get(id);
    if (state.timer) {
      stopPlayer(id);
    } else {
      startPlayer(id);
    }
  });
  range.addEventListener('input', () => {
    const state = playerStore.get(id);
    stopPlayer(id);
    state.stepIndex = Number(range.value);
    updatePlayer(id);
  });
  updatePlayer(id);
}

function renderAllocation(result) {
  const playerId = createPlayer('allocation', result);
  return `
    <div class="summary-grid">
      ${statCard('Algorithm', result.algorithm)}
      ${statCard('Successful allocations', result.successfulAllocations)}
      ${statCard('Failed allocations', result.failedAllocations)}
      ${statCard('Utilization', result.finalMetrics.utilization + '%')}
      ${statCard('Largest hole', result.finalMetrics.largestHole + ' KB')}
      ${statCard('External fragmentation', result.finalMetrics.externalFragmentation + ' KB')}
    </div>
    ${renderPlayerShell(playerId, `${result.algorithm} Playback`, 'Watch processes take and release memory step by step.')}
  `;
}

function renderAllocationCompare(results) {
  const cards = results.map((result) => `
    <article class="compare-card">
      <h3>${result.algorithm}</h3>
      <p>${allocationObservation(result)}</p>
      <div class="mini-grid">
        <div class="mini-card"><span class="muted">Successful</span><strong>${result.successfulAllocations}</strong></div>
        <div class="mini-card"><span class="muted">Failed</span><strong>${result.failedAllocations}</strong></div>
        <div class="mini-card"><span class="muted">Largest hole</span><strong>${result.finalMetrics.largestHole} KB</strong></div>
        <div class="mini-card"><span class="muted">Ext. frag.</span><strong>${result.finalMetrics.externalFragmentation} KB</strong></div>
      </div>
      ${renderMemoryBar(result.steps[result.steps.length - 1].blocks)}
    </article>
  `).join('');

  return `
    ${renderLegend()}
    <div class="compare-grid">${cards}</div>
  `;
}

function renderPaging(result) {
  const playerId = createPlayer('paging', result);
  return `
    <div class="summary-grid">
      ${statCard('Algorithm', result.algorithm)}
      ${statCard('Frames', result.frameCount)}
      ${statCard('Hits', result.hits)}
      ${statCard('Faults', result.faults)}
      ${statCard('Hit ratio', result.hitRatio + '%')}
    </div>
    ${renderPlayerShell(playerId, `${result.algorithm} Playback`, 'Watch pages enter frames and get replaced over time.')}
  `;
}

function renderPagingCompare(results) {
  const cards = results.map((result) => `
    <article class="compare-card">
      <h3>${result.algorithm}</h3>
      <p>${pagingObservation(result)}</p>
      <div class="mini-grid">
        <div class="mini-card"><span class="muted">Faults</span><strong>${result.faults}</strong></div>
        <div class="mini-card"><span class="muted">Hits</span><strong>${result.hits}</strong></div>
        <div class="mini-card"><span class="muted">Hit ratio</span><strong>${result.hitRatio}%</strong></div>
      </div>
      <div class="frame-row">
        ${result.steps[result.steps.length - 1].frames.map((frame, frameIndex) => `
          <div class="frame">
            <span class="muted">Frame ${frameIndex}</span>
            <strong>${frame === -1 ? '-' : frame}</strong>
          </div>
        `).join('')}
      </div>
    </article>
  `).join('');

  return `
    <div class="compare-grid">${cards}</div>
  `;
}

function renderTranslation(result) {
  const summary = `
    <div class="summary-grid">
      ${statCard('Page size', result.pageSize)}
      ${statCard('Logical address', result.logicalAddress)}
      ${statCard('Page number', result.pageNumber)}
      ${statCard('Offset', result.offset)}
      ${statCard('Frame', result.found ? result.frameNumber : 'Not in memory')}
      ${statCard('Physical address', result.found ? result.physicalAddress : 'Page fault')}
    </div>
  `;

  const table = `
    <table class="table">
      <thead>
        <tr><th>Page</th><th>Frame</th><th>Present</th></tr>
      </thead>
      <tbody>
        ${result.pageTable.map((entry) => `
          <tr>
            <td>${entry.page}</td>
            <td>${entry.frame === -1 ? '-' : entry.frame}</td>
            <td>${entry.present ? 'Yes' : 'No'}</td>
          </tr>
        `).join('')}
      </tbody>
    </table>
  `;

  return `
    ${summary}
    <article class="step">
      <div class="step-header">
        <div>
          <strong>Translation result</strong>
          <div>${result.found ? 'Address resolved successfully' : 'Address translation stopped'}</div>
        </div>
        <span class="chip ${result.found ? 'success' : 'warning'}">${result.found ? 'resolved' : 'page fault'}</span>
      </div>
      <p class="muted">${result.details}</p>
      ${table}
    </article>
  `;
}

function renderTlb(result) {
  return `
    <div class="summary-grid">
      ${statCard('Page number', result.pageNumber)}
      ${statCard('Offset', result.offset)}
      ${statCard('TLB hit', result.tlbHit ? 'Yes' : 'No')}
      ${statCard('Frame', result.pageFound ? result.frameNumber : 'Not in memory')}
      ${statCard('Physical address', result.pageFound ? result.physicalAddress : 'Page fault')}
    </div>
    <article class="step">
      <p class="muted">${result.details}</p>
    </article>
  `;
}

function renderDemandPaging(result) {
  const stepCards = result.steps.map((step, index) => `
    <article class="step">
      <div class="step-header">
        <div>
          <strong>Reference ${index + 1}</strong>
          <div>Page ${step.page}</div>
        </div>
        <span class="chip ${step.pageFault ? 'warning' : (step.tlbHit ? 'success' : 'info')}">
          ${step.tlbHit ? 'tlb hit' : step.pageFault ? 'page fault' : 'page hit'}
        </span>
      </div>
      <p class="muted">${step.details}</p>
      <div class="frame-row">
        ${step.frames.map((frame, frameIndex) => `
          <div class="frame ${step.loadedIntoFrame === frameIndex ? 'frame-focus' : ''}">
            <span class="muted">Frame ${frameIndex}</span>
            <strong>${frame === -1 ? '-' : frame}</strong>
          </div>
        `).join('')}
      </div>
      <div class="legend">
        ${step.tlb.map((entry) => `<span class="legend-item">TLB: page ${entry.page} -> frame ${entry.frame}</span>`).join('') || '<span class="legend-item">TLB empty</span>'}
      </div>
    </article>
  `).join('');

  return `
    <div class="summary-grid">
      ${statCard('Frames', result.frameCount)}
      ${statCard('TLB size', result.tlbSize)}
      ${statCard('TLB hits', result.tlbHits)}
      ${statCard('Page hits', result.pageHits)}
      ${statCard('Page faults', result.pageFaults)}
    </div>
    <div class="timeline">${stepCards}</div>
  `;
}

function renderSegmentation(result) {
  const table = `
    <table class="table">
      <thead><tr><th>Segment</th><th>Base</th><th>Limit</th></tr></thead>
      <tbody>
        ${result.segmentTable.map((entry) => `
          <tr>
            <td>${entry.segment}</td>
            <td>${entry.base}</td>
            <td>${entry.limit}</td>
          </tr>
        `).join('')}
      </tbody>
    </table>
  `;

  return `
    <div class="summary-grid">
      ${statCard('Segment number', result.segmentNumber)}
      ${statCard('Offset', result.offset)}
      ${statCard('Valid', result.valid ? 'Yes' : 'No')}
      ${statCard('Physical address', result.valid ? result.physicalAddress : 'Fault')}
    </div>
    <article class="step">
      <p class="muted">${result.details}</p>
    </article>
    ${table}
  `;
}

function renderMultiLevelPaging(result) {
  const table = `
    <table class="table">
      <thead><tr><th>Outer index</th><th>Inner index</th><th>Frame</th><th>Present</th></tr></thead>
      <tbody>
        ${result.pageMap.map((entry) => `
          <tr>
            <td>${entry.outerIndex}</td>
            <td>${entry.innerIndex}</td>
            <td>${entry.frame === -1 ? '-' : entry.frame}</td>
            <td>${entry.present ? 'Yes' : 'No'}</td>
          </tr>
        `).join('')}
      </tbody>
    </table>
  `;

  return `
    <div class="summary-grid">
      ${statCard('Page number', result.pageNumber)}
      ${statCard('Outer index', result.outerIndex)}
      ${statCard('Inner index', result.innerIndex)}
      ${statCard('Offset', result.offset)}
      ${statCard('Frame', result.found ? result.frameNumber : 'Not in memory')}
      ${statCard('Physical address', result.found ? result.physicalAddress : 'Page fault')}
    </div>
    <article class="step">
      <p class="muted">${result.details}</p>
    </article>
    ${table}
  `;
}

function renderThrashing(result) {
  const pointCards = result.points.map((point) => `
    <div class="compare-card">
      <h3>${point.frameCount} frames</h3>
      <div class="mini-grid">
        <div class="mini-card"><span class="muted">Faults</span><strong>${point.faults}</strong></div>
        <div class="mini-card"><span class="muted">Fault rate</span><strong>${point.faultRate}%</strong></div>
      </div>
    </div>
  `).join('');

  return `
    <div class="summary-grid">
      ${statCard('Algorithm', result.algorithm)}
      ${statCard('Threshold', result.threshold + '%')}
      ${statCard('Thrashing detected', result.detected ? 'Yes' : 'No')}
    </div>
    <article class="step">
      <p class="muted">${result.details}</p>
    </article>
    <div class="compare-grid">${pointCards}</div>
  `;
}

function hydratePlayers(target) {
  target.querySelectorAll('[data-player-id]').forEach((root) => {
    attachPlayer(root, root.dataset.playerId);
  });
}

function setLoading(target, message) {
  target.innerHTML = `<div class="loader">${message}</div>`;
}

async function postForm(endpoint, formData) {
  const response = await fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams(formData),
  });

  const text = await response.text();
  if (!response.ok) {
    throw new Error(text || 'Simulation failed.');
  }
  return JSON.parse(text);
}

async function submitSingle(form, endpoint, target, renderer, loadingMessage) {
  setLoading(target, loadingMessage);
  try {
    const data = await postForm(endpoint, new FormData(form));
    target.innerHTML = renderer(data);
    hydratePlayers(target);
  } catch (error) {
    target.innerHTML = `<div class="empty">${error.message}</div>`;
  }
}

async function submitCompare(form, endpoint, algorithms, target, renderer, loadingMessage) {
  setLoading(target, loadingMessage);
  try {
    const base = new FormData(form);
    const results = [];
    for (const algorithm of algorithms) {
      const payload = new FormData();
      for (const [key, value] of base.entries()) {
        payload.append(key, value);
      }
      payload.set('algorithm', algorithm);
      results.push(await postForm(endpoint, payload));
    }
    target.innerHTML = renderer(results);
  } catch (error) {
    target.innerHTML = `<div class="empty">${error.message}</div>`;
  }
}

function loadAllocationPreset() {
  const preset = allocationPresets[document.getElementById('allocation-preset').value];
  const form = document.getElementById('allocation-form');
  form.total_memory.value = preset.totalMemory;
  form.operations.value = preset.operations;
}

function loadPagingPreset() {
  const preset = pagingPresets[document.getElementById('paging-preset').value];
  const form = document.getElementById('paging-form');
  form.frame_count.value = preset.frameCount;
  form.reference_string.value = preset.referenceString;
}

function loadTranslationPreset() {
  const preset = translationPresets[document.getElementById('translation-preset').value];
  const form = document.getElementById('translation-form');
  form.page_size.value = preset.pageSize;
  form.logical_address.value = preset.logicalAddress;
  form.page_table.value = preset.pageTable;
}

document.getElementById('allocation-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/allocation', document.getElementById('allocation-result'), renderAllocation, 'Running allocation simulation...');
});

document.getElementById('paging-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/paging', document.getElementById('paging-result'), renderPaging, 'Running paging simulation...');
});

document.getElementById('translation-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/translation', document.getElementById('translation-result'), renderTranslation, 'Translating logical address...');
});

document.getElementById('tlb-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/tlb', document.getElementById('advanced-result'), renderTlb, 'Running TLB translation...');
});

document.getElementById('demand-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/demand-paging', document.getElementById('advanced-result'), renderDemandPaging, 'Running demand paging simulation...');
});

document.getElementById('segmentation-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/segmentation', document.getElementById('advanced-result'), renderSegmentation, 'Running segmentation simulation...');
});

document.getElementById('multi-level-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/multi-level-paging', document.getElementById('advanced-result'), renderMultiLevelPaging, 'Running two-level paging simulation...');
});

document.getElementById('thrashing-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  await submitSingle(event.currentTarget, '/simulate/thrashing', document.getElementById('advanced-result'), renderThrashing, 'Analyzing thrashing...');
});

document.getElementById('allocation-compare').addEventListener('click', async () => {
  await submitCompare(document.getElementById('allocation-form'), '/simulate/allocation', allocationAlgorithms, document.getElementById('allocation-result'), renderAllocationCompare, 'Comparing allocation algorithms...');
});

document.getElementById('paging-compare').addEventListener('click', async () => {
  await submitCompare(document.getElementById('paging-form'), '/simulate/paging', pagingAlgorithms, document.getElementById('paging-result'), renderPagingCompare, 'Comparing paging algorithms...');
});

document.getElementById('allocation-load-preset').addEventListener('click', loadAllocationPreset);
document.getElementById('paging-load-preset').addEventListener('click', loadPagingPreset);
document.getElementById('translation-load-preset').addEventListener('click', loadTranslationPreset);
document.getElementById('allocation-preset').addEventListener('change', loadAllocationPreset);
document.getElementById('paging-preset').addEventListener('change', loadPagingPreset);
document.getElementById('translation-preset').addEventListener('change', loadTranslationPreset);
