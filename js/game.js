const TILE_SIZE = 32;
const BASE_FRAME_DURATION = 50; // 0.05s 一帧
const FRAMES_PER_MOVE = 8;       // 走一格需要 8 帧
const BASE_MOVE_DURATION = BASE_FRAME_DURATION * FRAMES_PER_MOVE; // 800ms

let currentTab = 'carrot';
let currentLevelIndex = 0;
let map = [];
let rows = 0;
let cols = 0;
// 在文件头部 moveState 附近增加
let lastInputTime = 0; // 上次操作时间 (用于Idle检测)
let animState = 'static'; // static, moving, idle, dying, fading
let animStartTime = 0; // 动画开始时间 (用于 Death/Fade)
// 逻辑坐标 (Grid)
let player = { x: 0, y: 0 };
// 渲染坐标 (Pixel)
let visual = { x: 0, y: 0 };
// 移动状态
let moveState = {
  isMoving: false,
  startX: 0, startY: 0, // 像素坐标
  targetX: 0, targetY: 0, // 像素坐标
  startTime: 0,
  duration: BASE_MOVE_DURATION
};
// ... 其他变量 ...
let hasMovedOnce = false; // ★ 新增：标记玩家是否移动过
let inventory = { s: false, g: false, c: false };
let stats = {
  carrotsTotal: 0, carrotsCollected: 0,
  eggsTotal: 0, eggsPlanted: 0,
  steps: 0, gameTimeAccumulator: 0,
  moveHistory: []
};

let gameState = 'stopped'; // stopped, playing, won, dead
let timeScale = 1.0;
let lastFrameTime = 0;

let isReplaying = false;
let isCpuReplay = false;
let replayQueue = []; 
let replayStepIndex = 0;

const STORAGE_KEY = 'bobby_game_records';
const ASSET_SOURCES = [
  'src/tileset.png',
  'src/BobbyCarrot.png',
  'src/bobby_left.png',
  'src/bobby_right.png',
  'src/bobby_up.png',
  'src/bobby_down.png',
  'src/bobby_idle.png',
  'src/bobby_death.png',
  'src/bobby_fade.png',
  'src/tile_conveyor_left.png',
  'src/tile_conveyor_right.png',
  'src/tile_conveyor_up.png',
  'src/tile_conveyor_down.png',
  'src/tile_finish.png'
];

function preloadAssets(sources) {
  return Promise.all(sources.map(src => {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.src = src;
      img.onload = resolve;
      img.onerror = () => {
        console.warn(`无法加载资源: ${src}`);
        resolve(); 
      };
    });
  }));
}

window.addEventListener('load', () => {
  setupMobileControls();
  const gameGrid = document.getElementById('game-grid');
  if (gameGrid) gameGrid.innerHTML = '<div style="color:white;padding:20px;">资源加载中...</div>';
  preloadAssets(ASSET_SOURCES).then(() => {
    if (typeof AUTO_SOLVED_PATHS !== 'undefined')
    switchTab('carrot');
    requestAnimationFrame(gameLoop);
  });
});

function gameLoop(timestamp) {
  const dt = timestamp - lastFrameTime;
  const safeDt = Math.min(dt, 100);
  lastFrameTime = timestamp;

  renderAnimations(timestamp);
  if (gameState === 'playing') {

    if ((isReplaying || isCpuReplay) && !moveState.isMoving) {
      processReplayQueue();
    }

    updateMovementLogic(timestamp);

    if (!isReplaying && !isCpuReplay) {
      stats.gameTimeAccumulator += safeDt * timeScale;
      const t = (stats.gameTimeAccumulator / 1000).toFixed(1);
      document.getElementById('disp-time').innerText = t + 's';
    } else {
      document.getElementById('disp-time').innerText = isCpuReplay ? "(参考演示)" : "(回放)";
    }
  }

  requestAnimationFrame(gameLoop);
}

function toggleSpeed() {
  if (timeScale === 1.0) {
    timeScale = 2.0;
    document.getElementById('speed-btn').innerText = "⏩ x2.0";
  } else {
    timeScale = 1.0;
    document.getElementById('speed-btn').innerText = "⏩ x1.0";
  }
  // ★ 新增：同步更新 CSS 变量，让传送带和终点动画也加速
  document.documentElement.style.setProperty('--anim-speed', timeScale);
}

function tryMoveInput(dx, dy) {
  if (gameState !== 'playing') return false;
  if (moveState.isMoving) return false;
  lastInputTime = Date.now();
  if (animState === 'idle') animState = 'static';
  hasMovedOnce = true;
  if (dx === -1) moveState.direction = 'left';
  else if (dx === 1) moveState.direction = 'right';
  else if (dy === -1) moveState.direction = 'up';
  else if (dy === 1) moveState.direction = 'down';

  const nx = player.x + dx;
  const ny = player.y + dy;

  if (!canMoveLogic(player.x, player.y, dx, dy)) {
    // 即使走不动，也原地转身
    updatePlayerSprite(0);
    return false;
  }

  // 4. 记录步骤
  if (!isReplaying && !isCpuReplay) {
    stats.moveHistory.push({ dx, dy });
  }
  stats.steps++;

  // ★ 5. 标记状态并启动
  animState = 'moving';
  startMoveAnimation(nx, ny);
  return true;
}

// 2. 纯逻辑检查 (只读，不修改任何状态！)
function canMoveLogic(cx, cy, dx, dy) {
  const nx = cx + dx;
  const ny = cy + dy;

  // 越界
  if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) return false;

  const currentCh = map[cy][cx];
  const nextCh = map[ny][nx];

  // 离开检查
  if (isCorner(currentCh)) {
    if (!canExitCorner(currentCh, player.cornerEntry, dx, dy)) return false;
  }
  if (currentCh === '|' && dx !== 0) return false;
  if (currentCh === '-' && dy !== 0) return false;

  // 进入检查
  if (['=', '.', 'E'].includes(nextCh)) return false;
  if (nextCh === '|' && dx !== 0) return false;
  if (nextCh === '-' && dy !== 0) return false;
  if (nextCh === 'S' && !inventory.s) return false;
  if (nextCh === 'G' && !inventory.g) return false;
  if (nextCh === 'C' && !inventory.c) return false;

  if (isCorner(nextCh)) {
    if (!canEnterCorner(nextCh, dx, dy)) return false;
  }

  // ★ 移除所有状态修改代码！只返回 true/false
  return true;
}

// 初始化移动参数
function startMoveAnimation(targetGridX, targetGridY) {
  moveState.isMoving = true;
  moveState.startX = visual.x;
  moveState.startY = visual.y;
  moveState.targetX = targetGridX * TILE_SIZE;
  moveState.targetY = targetGridY * TILE_SIZE;
  moveState.startTime = performance.now();
  moveState.duration = BASE_MOVE_DURATION / timeScale;

  // 逻辑坐标先行更新 (为了防止连续按键时的逻辑错乱，虽然现在加了锁)
  // 但为了配合"离开后触发"机制，我们需要记录"上一格"
  // 这里我们暂存"即将离开的格子"和"即将进入的格子"
  // 真正的地图修改推迟到动画结束 (processArrival)
}

// 每帧更新位置
function updateMovementLogic(now) {
  if (!moveState.isMoving) return;

  const elapsed = now - moveState.startTime;
  // 计算进度 0.0 ~ 1.0
  const progress = Math.min(elapsed / moveState.duration, 1.0);

  // 线性插值计算渲染坐标
  visual.x = moveState.startX + (moveState.targetX - moveState.startX) * progress;
  visual.y = moveState.startY + (moveState.targetY - moveState.startY) * progress;

  // 移动结束结算
  if (progress >= 1.0) {
    moveState.isMoving = false;
    processArrival();
  }
}
// 移动结束后的逻辑结算 (Post-Move Effects)
function processArrival() {
  // 1. 计算 grid 坐标变化
  // 因为视觉坐标已经到了目标，我们可以反推逻辑坐标
  // 或者更简单的：我们只允许走直线，利用 startX/Y 和 targetX/Y 算出 dx, dy
  const px = Math.round(moveState.startX / TILE_SIZE);
  const py = Math.round(moveState.startY / TILE_SIZE);
  const nx = Math.round(moveState.targetX / TILE_SIZE);
  const ny = Math.round(moveState.targetY / TILE_SIZE);

  const dx = nx - px;
  const dy = ny - py;

  const currentCh = map[py][px]; // 刚才离开的格子
  const nextCh = map[ny][nx];    // 现在踩着的格子

  // 2. 更新玩家逻辑坐标
  player.x = nx;
  player.y = ny;

  // 3. 处理"离开"效果 (修改 map[py][px])
  let mapChanged = false;
  if (isCorner(currentCh)) { map[py][px] = rotateCorner(currentCh); mapChanged = true; }
  else if (currentCh === '|') { map[py][px] = '-'; mapChanged = true; }
  else if (currentCh === '-') { map[py][px] = '|'; mapChanged = true; }
  else if (currentCh === 'x') { map[py][px] = 'X'; mapChanged = true; }
  else if (currentCh === 'e') { map[py][px] = 'E'; stats.eggsPlanted++; mapChanged = true; }

  // 4. 处理"进入"效果 (修改 map[ny][nx])
  if (isCorner(nextCh)) player.cornerEntry = getSide(dx, dy);
  else player.cornerEntry = null;

  if (nextCh === '*') { stats.carrotsCollected++; map[ny][nx] = '+'; mapChanged = true; }
  else if (nextCh === 's') { inventory.s = true; map[ny][nx] = ' '; mapChanged = true; }
  else if (nextCh === 'g') { inventory.g = true; map[ny][nx] = ' '; mapChanged = true; }
  else if (nextCh === 'c') { inventory.c = true; map[ny][nx] = ' '; mapChanged = true; }
  else if (['S', 'G', 'C'].includes(nextCh)) { map[ny][nx] = ' '; mapChanged = true; }
  else if (nextCh === 'R') { triggerRedButton(); mapChanged = true; }
  else if (nextCh === 'Y') { triggerYellowButton(); mapChanged = true; }
  else if (nextCh === 'X') { die("你掉进了陷阱！"); return; } // 死亡直接返回

  if (mapChanged) drawGrid(); // 只有地图变了才重绘网格，玩家位置单独由 DOM 控制
  updateHud();

  // 5. 检查胜利
  if (map[ny][nx] === 'o') {
    // 检查是否也是传送带？通常终点不是传送带。
    checkWin();
  }

  // 6. 检查传送带 (链式反应)
  const beltDir = getBeltDir(map[ny][nx]);
  if (beltDir) {
    // 尝试发起下一次移动 (被动)
    const bdx = beltDir.dx;
    const bdy = beltDir.dy;

    // 检查传送带前方是否受阻
    // 注意：这里复用 canMoveLogic，如果前方受阻，就停在传送带上
    if (canMoveLogic(nx, ny, bdx, bdy)) {
      // ★ 立即启动下一段移动，无需等待
      startMoveAnimation(nx + bdx, ny + bdy);
    }
  }
}

// 统一渲染函数：根据当前状态决定显示什么
// ================= 渲染核心：根据状态决定显示哪张图、哪一帧 =================
function renderAnimations(now) {
  const el = document.getElementById('player-sprite');
  if (!el) return;

  // 1. 始终更新位移
  el.style.transform = `translate(${visual.x}px, ${visual.y}px)`;

  // === 优先级 1: 死亡 (dying) ===
  if (animState === 'dying') {
    el.className = 'anim-death';
    const elapsed = now - animStartTime;
    let frame = Math.floor(elapsed / 100);
    if (frame > 7) frame = 7;
    el.style.backgroundPosition = `-${frame * 44}px 0`;
    return;
  }

  // === 优先级 2: 通关 (fading) ===
  if (animState === 'fading') {
    el.className = 'anim-fade';
    const elapsed = now - animStartTime;
    let frame = Math.floor(elapsed / 100);
    if (frame > 8) frame = 8;
    el.style.backgroundPosition = `-${frame * 36}px 0`;
    return;
  }

  // === 优先级 3: 移动 (moving) ===
  if (moveState.isMoving) {
    el.className = `anim-move anim-${moveState.direction}`;

    const elapsed = now - moveState.startTime;
    const progress = Math.min(elapsed / moveState.duration, 1.0);

    // 计算原始帧索引 0~7
    // 使用 Math.floor 确保分布均匀，并在最后时刻限制为 7
    let rawFrame = Math.floor(progress * 8);
    if (rawFrame > 7) rawFrame = 7;

    let finalFrame = rawFrame;

    // ★ 特殊循环逻辑：Left/Right 从第5帧(idx 4)开始，循环到第4帧(idx 3)
    // 序列: 4, 5, 6, 7, 0, 1, 2, 3
    if (['left', 'right'].includes(moveState.direction)) {
      finalFrame = (rawFrame + 4) % 8;
    }
    // Up/Down 不需要处理，默认就是 0 -> 7

    el.style.backgroundPosition = `-${finalFrame * 36}px 0`;
    return;
  }

  // === 优先级 4: 待机动画 (idle) ===
  // 只有在长时间未操作，且不是刚开局时播放
  if (gameState === 'playing' && hasMovedOnce && (Date.now() - lastInputTime > 5000)) {
    animState = 'idle';
    el.className = 'anim-idle-action';
    const idleFrame = Math.floor(now / 200) % 3;
    el.style.backgroundPosition = `-${idleFrame * 36}px 0`;
    return;
  }

  // === 优先级 5: 定格状态 (Directional Static) ===
  // 移动结束了，但还没进入 Idle，保持朝向

  // 情况 A: 游戏刚开始，一步没走 -> 显示默认正面图
  if (!hasMovedOnce) {
    animState = 'static';
    el.className = 'anim-static';
    el.style.backgroundPosition = '0 0';
    return;
  }

  // 情况 B: 走过了，停留在最后的方向
  animState = 'static';
  el.className = `anim-move anim-${moveState.direction}`; // 复用移动的贴图类

  let stopFrame = 0;

  if (['left', 'right'].includes(moveState.direction)) {
    // Left/Right: 停在第 4 个 (Index 3)
    stopFrame = 3;
  } else {
    // Up/Down: 停在最后一帧 (Index 7)
    stopFrame = 7;
  }

  el.style.backgroundPosition = `-${stopFrame * 36}px 0`;
}
// ================= 辅助逻辑 =================

function isCorner(ch) { return ['⌝', '⌟', '⌞', '⌜'].includes(ch); }
function rotateCorner(ch) {
  const m = { '⌝': '⌟', '⌟': '⌞', '⌞': '⌜', '⌜': '⌝' };
  return m[ch] || ch;
}
function getBeltDir(ch) {
  if (ch === '<') return { dx: -1, dy: 0 };
  if (ch === '>') return { dx: 1, dy: 0 };
  if (ch === '^') return { dx: 0, dy: -1 };
  if (ch === 'v') return { dx: 0, dy: 1 };
  return null;
}
function getSide(dx, dy) {
  if (dx === 1) return 'L'; if (dx === -1) return 'R';
  if (dy === 1) return 'U'; if (dy === -1) return 'D';
  return null;
}
function canExitCorner(ch, entrySide, dx, dy) {
  if (entrySide === 'L' && dx === -1) return true;
  if (entrySide === 'R' && dx === 1) return true;
  if (entrySide === 'U' && dy === -1) return true;
  if (entrySide === 'D' && dy === 1) return true;

  if (ch === '⌝') return (entrySide === 'L' && dy === 1) || (entrySide === 'D' && dx === -1);
  if (ch === '⌟') return (entrySide === 'L' && dy === -1) || (entrySide === 'U' && dx === -1);
  if (ch === '⌞') return (entrySide === 'R' && dy === -1) || (entrySide === 'U' && dx === 1);
  if (ch === '⌜') return (entrySide === 'R' && dy === 1) || (entrySide === 'D' && dx === 1);
  return false;
}
function canEnterCorner(ch, dx, dy) {
  const side = getSide(dx, dy);
  if (ch === '⌝') return side === 'L' || side === 'D';
  if (ch === '⌟') return side === 'L' || side === 'U';
  if (ch === '⌞') return side === 'R' || side === 'U';
  if (ch === '⌜') return side === 'R' || side === 'D';
  return false;
}

function triggerRedButton() {
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x < cols; x++) {
      let c = map[y][x];
      if (c === 'R') c = 'r';
      else if (c === 'r') c = 'R';
      else if (c === '|') c = '-';
      else if (c === '-') c = '|';
      else if (isCorner(c)) c = rotateCorner(c);
      map[y][x] = c;
    }
  }
}
function triggerYellowButton() {
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x < cols; x++) {
      let c = map[y][x];
      if (c === 'Y') c = 'y';
      else if (c === 'y') c = 'Y';
      else if (c === '<') c = '>';
      else if (c === '>') c = '<';
      else if (c === '^') c = 'v';
      else if (c === 'v') c = '^';
      map[y][x] = c;
    }
  }
}

// ================= 初始化与 UI =================

function startGame(index, prepReplay = false, cpuMode = false) {
  hasMovedOnce = false;
  currentLevelIndex = index;
  document.getElementById('menu-screen').classList.add('hidden');
  document.getElementById('game-screen').classList.remove('hidden');
  document.getElementById('msg-overlay').classList.add('hidden');

  isCpuReplay = cpuMode;
  isReplaying = prepReplay;
  replayStepIndex = 0;
  replayQueue = []; // 清空队列

  initLevelData();
  drawGrid();
  updateHud();

  // ★ 强制重置视觉坐标，防止上一局的残影
  visual.x = player.x * TILE_SIZE;
  visual.y = player.y * TILE_SIZE;

  // ★ 立即渲染一次，确保开局有图
  lastInputTime = Date.now();
  animState = 'static';
  renderAnimations(performance.now()); // 强制渲染第一帧

  gameState = 'playing';
  stats.gameStartTime = Date.now();
  updateControlsVisibility(); 
}

function initLevelData() {
  const rawLevel = (currentTab === 'carrot' ? CARROT_LEVELS : EGG_LEVELS)[currentLevelIndex];
  rows = rawLevel.length;
  cols = rawLevel[0].length;
  map = [];

  stats = {
    carrotsTotal: 0, carrotsCollected: 0,
    eggsTotal: 0, eggsPlanted: 0,
    steps: 0,
    gameTimeAccumulator: 0, // ★ 修改：初始化为 0
    moveHistory: []
  };

  inventory = { s: false, g: false, c: false };
  player.cornerEntry = null;
  moveState.isMoving = false;

  let startPos = { x: 0, y: 0 };

  for (let y = 0; y < rows; y++) {
    const rowArr = rawLevel[y].split('');
    map.push(rowArr);
    for (let x = 0; x < cols; x++) {
      const ch = rowArr[x];
      if (ch === '@') {
        startPos = { x, y };
        rowArr[x] = ' ';
      } else if (ch === '*') {
        stats.carrotsTotal++;
      } else if (ch === 'e') {
        stats.eggsTotal++;
      }
    }
  }

  player.x = startPos.x;
  player.y = startPos.y;
  visual.x = startPos.x * TILE_SIZE;
  visual.y = startPos.y * TILE_SIZE;
  // 全局变量，用于渲染起点地板
  window.levelStartPos = startPos;
}
function goToNextLevel() {
  // 1. 关闭结算弹窗
  document.getElementById('msg-overlay').classList.add('hidden');
  
  // 2. 获取当前模式的关卡列表
  const levels = currentTab === 'carrot' ? CARROT_LEVELS : EGG_LEVELS;
  
  // 3. 计算下一关索引
  let nextIdx = currentLevelIndex + 1;
  
  // 4. 如果是最后一关，回到第一关 (Loop)
  if (nextIdx >= levels.length) {
    nextIdx = 0;
    // 可选：提示一下回到开头了
    // alert("恭喜通关！回到第一关。"); 
  }
  
  // 5. 启动新关卡
  startGame(nextIdx, false, false);
}
function restartLevel() {
  // 1. 强制关闭所有 UI 菜单
  document.getElementById('msg-overlay').classList.add('hidden');
  document.getElementById('pause-menu').classList.add('hidden');
  
  // 2. 如果已经在看回放、AI演示，或者已经死了/赢了，直接重开，不播动画
  // (因为这时候播死亡动画会很怪，或者状态不对)
  if (isReplaying || isCpuReplay || gameState === 'dead' || gameState === 'won') {
    startGame(currentLevelIndex, isReplaying, isCpuReplay);
    return;
  }

  // 3. 正常游戏中重开 -> 播放死亡动画
  // 切换状态，防止继续移动
  gameState = 'dead'; 
  
  // 设置动画状态
  animState = 'dying';
  animStartTime = performance.now();
  
  // 关键：立即刷新一次渲染，确保玩家看到切换到了死亡贴图
  // 如果不调这个，可能要等到下一帧 requestAnimationFrame 才变，会有瞬间延迟
  const el = document.getElementById('player-sprite');
  if (el) {
    el.className = 'anim-death';
    el.style.backgroundPosition = '0 0';
  }

  // 4. 等待动画播放完毕 (0.8秒) 后执行真正的重置
  setTimeout(() => {
    // 再次检查，防止玩家在这 0.8s 内狂按 ESC 退出了
    if (gameState === 'dead') { 
        startGame(currentLevelIndex, isReplaying, isCpuReplay);
    }
  }, 800);
}

function backToMenu() {
  gameState = 'stopped';

  // ★ 新增：强制关闭弹窗
  document.getElementById('msg-overlay').classList.add('hidden');
  document.getElementById('pause-menu').classList.add('hidden');
  document.getElementById('game-screen').classList.add('hidden');
  document.getElementById('menu-screen').classList.remove('hidden');
  renderLevelList();
  updateControlsVisibility(); 
}
function checkWin() {
  if (stats.carrotsCollected === stats.carrotsTotal &&
    stats.eggsPlanted === stats.eggsTotal) {

    gameState = 'won'; // 逻辑胜利，锁住操作

    // ★ 切换到消失动画
    animState = 'fading';
    animStartTime = performance.now();

    // 计算最终时间
    const finalTime = (stats.gameTimeAccumulator / 1000).toFixed(1);
    if (!isReplaying && !isCpuReplay) {
      saveRecord(currentTab, currentLevelIndex, stats.steps, finalTime, stats.moveHistory);
    }

    // 0.9s (9帧 * 0.1s) 后显示结算窗口
    setTimeout(() => {
      showWinMsg(finalTime);
    }, 900)
  updateControlsVisibility(); 
  }
}

function die(msg) {
  if (gameState === 'dead') return;

  gameState = 'dead'; 
  updateControlsVisibility(); 
  animState = 'dying';
  animStartTime = performance.now();
  setTimeout(() => {
    alert(msg + " 按 R 重试。");
  }, 1000);
}

function showWinMsg(timeStr) {
  const overlay = document.getElementById('msg-overlay');
  const content = document.getElementById('msg-content');
  const title = document.getElementById('msg-title');
  const btnReplay = document.getElementById('btn-replay-win');

  overlay.classList.remove('hidden');

  if (isCpuReplay) {
    title.innerText = "🤖 参考 演示结束";
    content.innerHTML = `<p>解法步数: ${stats.steps}</p>`;
    btnReplay.classList.add('hidden');
  } else if (isReplaying) {
    title.innerText = "▶ 回放结束";
    content.innerHTML = `<p>演示完成。</p>`;
    btnReplay.classList.add('hidden');
  } else {
    title.innerText = "🎉 关卡完成!";
    content.innerHTML = `
      <div class="stat-row"><span>步数:</span> <b>${stats.steps}</b></div>
      <div class="stat-row"><span>时间:</span> <b>${timeStr}s</b></div>
    `;
    btnReplay.classList.remove('hidden');
  }
}

// ================= 回放系统 =================

function parseSolutionPath(pathStr) {
  const moves = [];
  for (let char of pathStr) {
    if (char === 'U') moves.push({ dx: 0, dy: -1 });
    else if (char === 'D') moves.push({ dx: 0, dy: 1 });
    else if (char === 'L') moves.push({ dx: -1, dy: 0 });
    else if (char === 'R') moves.push({ dx: 1, dy: 0 });
  }
  return moves;
}

function watchCpuReplay(type, idx) {
  if (typeof AUTO_SOLVED_PATHS === 'undefined') return;
  const pathStr = AUTO_SOLVED_PATHS[type][idx];
  if (!pathStr) return;

  // 启动游戏
  currentTab = type;
  startGame(idx, true, true); // prepReplay=true, cpuMode=true

  // 填充队列
  replayQueue = parseSolutionPath(pathStr);
}
function startReplay() {
  watchReplay(currentTab, currentLevelIndex);
}
function watchReplay(type, idx) {
  const records = getRecords();
  const rec = records[type][idx];
  if (!rec || !rec.solution) return;

  currentTab = type;
  startGame(idx, true, false); // prepReplay=true, cpuMode=false

  replayQueue = rec.solution; // 直接使用存储的 [{dx,dy}, ...]
}

// 修改 src/game.js 中的 processReplayQueue
function processReplayQueue() {
  if (replayStepIndex >= replayQueue.length) return;

  const move = replayQueue[replayStepIndex];

  if (!moveState.isMoving) {
    // ★ 修改：只有移动成功启动了，才进阶到下一步
    if (tryMoveInput(move.dx, move.dy)) {
      replayStepIndex++;
    } else {
      // ★ 如果这一步走不通（撞墙了），说明录像坏了，强制停止回放
      console.warn(`回放数据同步错误 (Step ${replayStepIndex})，停止回放。`);
      isReplaying = false;
      isCpuReplay = false;
      // 可选：弹出提示 alert("录像文件已损坏或版本不匹配");
    }
  }
}

// ================= 存档/菜单逻辑 =================
// (保留原来的 localStorage, export/import, renderLevelList 等逻辑)
// 为节省篇幅，这里假设那些辅助函数依然存在。
// 务必将之前的 renderLevelList, getRecords, saveRecord, exportSave, importSave, switchTab 等函数保留在文件底部！

// (此处补全缺失的辅助函数...)
function getRecords() {
  const s = localStorage.getItem(STORAGE_KEY);
  return s ? JSON.parse(s) : { carrot: {}, egg: {} };
}
function saveRecord(type, idx, steps, timeStr, history) {
  const records = getRecords();
  if (!records[type]) records[type] = {};
  const old = records[type][idx];
  const timeVal = parseFloat(timeStr);
  let isBetter = false;
  if (!old) isBetter = true;
  else if (steps < old.steps) isBetter = true;
  else if (steps === old.steps && timeVal < old.time) isBetter = true;

  if (isBetter) {
    records[type][idx] = { steps, time: timeVal, solution: history };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(records));
  }
}
function updateHud() {
  document.getElementById('disp-steps').innerText = stats.steps;
  document.getElementById('disp-carrot').innerText = (stats.carrotsTotal - stats.carrotsCollected);
  document.getElementById('disp-egg').innerText = (stats.eggsTotal - stats.eggsPlanted);
  let keys = [];
  if (inventory.s) keys.push('<span class="t-key-s" style="display:inline-block;width:16px;height:16px;background-size:128px 96px;background-position:0 -64px;vertical-align:middle"></span>'); // 简化显示
  else document.getElementById('disp-keys').innerText = Object.values(inventory).some(k => k) ? 'Yes' : '-';
  checkFinishAnimation();
}
function checkFinishAnimation() {
  const allCollected = (stats.carrotsCollected === stats.carrotsTotal) &&
    (stats.eggsPlanted === stats.eggsTotal);

  const ends = document.querySelectorAll('.js-finish-cell');
  ends.forEach(el => {
    if (allCollected) {
      el.classList.add('t-end-active'); // 激活 CSS 动画
    } else {
      el.classList.remove('t-end-active');
    }
  });
}
// 简化的 HUD 更新，实际图标样式需 CSS 支持，这里暂且用文字或简化
// 重新实现 drawGrid 依赖的渲染函数
function drawGrid() {
  const grid = document.getElementById('game-grid');
  grid.style.gridTemplateColumns = `repeat(${cols}, 32px)`;
  grid.innerHTML = '';
  for (let y = 0; y < rows; y++) {
    for (let x = 0; x < cols; x++) {
      const el = document.createElement('div');
      el.className = 'cell';
      const ch = map[y][x];

      // 起点地板特殊处理
      if (window.levelStartPos && x === window.levelStartPos.x && y === window.levelStartPos.y && ch === ' ') {
        el.classList.add('t-start');
      } else {
        switch (ch) {
          case '.': el.classList.add(getGrassClass(x, y)); break;
          case ' ': el.classList.add('t-ground'); break;
          case '=': el.classList.add(getFenceClass(x, y)); break;
          case '*': el.classList.add('t-carrot'); break;
          case '+': el.classList.add('t-carrot-eaten'); break;
          case 'o':
            el.classList.add('t-end', 'js-finish-cell');
            break;
          case 'x': el.classList.add('t-trap-off'); break;
          case 'X': el.classList.add('t-trap-on'); break;
          case '<':
            el.classList.add('t-belt-l', 'animated', 'anim-loop');
            break;
          case '>':
            el.classList.add('t-belt-r', 'animated', 'anim-loop');
            break;
          case '^':
            el.classList.add('t-belt-u', 'animated', 'anim-loop');
            break;
          case 'v':
            el.classList.add('t-belt-d', 'animated', 'anim-loop');
            break;
          case '|': el.classList.add('t-w-ver'); break;
          case '-': el.classList.add('t-w-hor'); break;
          case '⌜': el.classList.add('t-w-lu'); break;
          case '⌝': el.classList.add('t-w-ru'); break;
          case '⌟': el.classList.add('t-w-rd'); break;
          case '⌞': el.classList.add('t-w-ld'); break;
          case 'R': el.classList.add('t-btn-r-off'); break;
          case 'r': el.classList.add('t-btn-r-on'); break;
          case 'Y': el.classList.add('t-btn-y-off'); break;
          case 'y': el.classList.add('t-btn-y-on'); break;
          case 's': el.classList.add('t-key-s'); break;
          case 'S': el.classList.add('t-lock-s'); break;
          case 'g': el.classList.add('t-key-g'); break;
          case 'G': el.classList.add('t-lock-g'); break;
          case 'c': el.classList.add('t-key-c'); break;
          case 'C': el.classList.add('t-lock-c'); break;
          case 'e': el.classList.add('t-egg-spot'); break;
          case 'E': el.classList.add('t-egg-done'); break;
          default: el.classList.add('t-ground'); break;
        }
      }
      grid.appendChild(el);
    }
  }
}

// 复制之前的 getGrassClass, isGrassConnect, getFenceClass, isFence 函数到这里
function isGrassConnect(x, y) {
  if (x < 0 || x >= cols || y < 0 || y >= rows) return true;
  if (map[y][x] === '.') return true;
  return false;
}
function getGrassClass(x, y) {
  const u = isGrassConnect(x, y - 1);
  const d = isGrassConnect(x, y + 1);
  const l = isGrassConnect(x - 1, y);
  const r = isGrassConnect(x + 1, y);
  if (u && d && l && r) return 't-grass-full';
  if (u && d && l && !r) return 't-grass-no-r';
  if (u && d && !l && r) return 't-grass-no-l';
  if (u && !d && l && r) return 't-grass-no-d';
  if (!u && d && l && r) return 't-grass-no-u';
  if (!u && !d && l && r) return 't-grass-no-ud';
  if (u && d && !l && !r) return 't-grass-no-lr';
  if (u && !d && l && !r) return 't-grass-no-rd';
  if (u && !d && !l && r) return 't-grass-no-ld';
  if (!u && d && !l && r) return 't-grass-no-lu';
  if (!u && d && l && !r) return 't-grass-no-ru';
  if (u && !d && !l && !r) return 't-grass-iso';
  if (!u && d && !l && !r) return 't-grass-no-u';
  if (!u && !d && l && !r) return 't-grass-no-rd';
  if (!u && !d && !l && r) return 't-grass-no-ld';
  return 't-grass-full';
}
function isFence(x, y) {
  if (x < 0 || x >= cols || y < 0 || y >= rows) return false;
  return map[y][x] === '=';
}
function getFenceClass(x, y) {
  const u = isFence(x, y - 1);
  const d = isFence(x, y + 1);
  const l = isFence(x - 1, y);
  const r = isFence(x + 1, y);
  if (l && r) return 't-fence-h';
  if (r && d) return 't-fence-rd';
  if (l && d) return 't-fence-ld';
  if ((r && u) || (r && !u && !d && !l)) return 't-fence-ru';
  if ((l && u) || (l && !u && !d && !r)) return 't-fence-lu';
  if ((u && d) || (u && !d) || (!u && d)) return 't-fence-v';
  return 't-fence-h';
}

function switchTab(tab) {
  currentTab = tab;
  document.querySelectorAll('.tab-btn').forEach(b => {
    b.classList.toggle('active', b.innerText.includes(tab === 'carrot' ? '胡萝卜' : '彩蛋'));
  });
  renderLevelList();
}

function renderLevelList() {
  const listEl = document.getElementById('level-list');
  listEl.innerHTML = '';
  const levels = currentTab === 'carrot' ? CARROT_LEVELS : EGG_LEVELS;
  const records = getRecords()[currentTab] || {};
  const hasSolutions = (typeof AUTO_SOLVED_PATHS !== 'undefined');

  levels.forEach((lvl, idx) => {
    const card = document.createElement('div');
    card.className = 'level-card';
    const rec = records[idx];
    if (rec) card.classList.add('completed');

    let html = `<div class="level-title">关卡 ${idx + 1}</div>`;
    if (rec) {
      html += `<div class="level-stats">★ ${rec.steps}步<br>⏱ ${rec.time}s</div>`;
      if (rec.solution) {
        html += `<button class="replay-btn-mini" onclick="event.stopPropagation(); watchReplay('${currentTab}', ${idx})">▶ 我的回放</button>`;
      }
    } else {
      html += `<div class="level-stats">未完成</div>`;
    }
    if (hasSolutions && AUTO_SOLVED_PATHS[currentTab] && AUTO_SOLVED_PATHS[currentTab][idx]) {
      html += `<button class="replay-btn-cpu" onclick="event.stopPropagation(); watchCpuReplay('${currentTab}', ${idx})">🤖 参考 演示</button>`;
    }

    card.innerHTML = html;
    card.onclick = () => startGame(idx);
    listEl.appendChild(card);
  });
}

function exportSave() {
  const data = localStorage.getItem(STORAGE_KEY);
  if (!data) { alert("无存档"); return; }
  const blob = new Blob([data], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `bobby_save.json`;
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
}
function importSave(input) {
  const file = input.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = function (e) {
    try {
      JSON.parse(e.target.result);
      localStorage.setItem(STORAGE_KEY, e.target.result);
      location.reload();
    } catch (err) { alert("存档损坏"); }
  };
  reader.readAsText(file);
}
function closeMsg() { document.getElementById('msg-overlay').classList.add('hidden'); }
function togglePause() {
  if (gameState !== 'playing' && gameState !== 'paused') return;

  const menu = document.getElementById('pause-menu');

  if (gameState === 'playing') {
    gameState = 'paused';
    menu.classList.remove('hidden');
  } else {
    gameState = 'playing';
    menu.classList.add('hidden');
    lastFrameTime = performance.now();
  }
  updateControlsVisibility(); 
}

function setupMobileControls() {
  const bindBtn = (id, dx, dy) => {
    const btn = document.getElementById(id);
    if (!btn) return;
    
    const handleMove = (e) => {
      e.preventDefault();
      if (gameState === 'playing' && !isReplaying && !isCpuReplay) {
        tryMoveInput(dx, dy);
      }
    };

    btn.addEventListener('touchstart', handleMove, { passive: false });
    btn.addEventListener('mousedown', handleMove); 
  };

  bindBtn('btn-up', 0, -1);
  bindBtn('btn-down', 0, 1);
  bindBtn('btn-left', -1, 0);
  bindBtn('btn-right', 1, 0);
}

function updateControlsVisibility() {
  const controls = document.getElementById('mobile-controls');
  if (!controls) return;
  const shouldShow = (gameState === 'playing') && !isReplaying && !isCpuReplay;

  if (shouldShow) {
    controls.classList.remove('hidden');
  } else {
    controls.classList.add('hidden');
  }
}

window.addEventListener('keydown', (e) => {
  if (e.key === 'r' || e.key === 'R') { restartLevel(); return; }
  if (e.key === 'Escape') {
    if (gameState === 'playing' || gameState === 'paused') {
      togglePause();
    } else {
      backToMenu();
    }
    return;
  }
  if (gameState !== 'playing' || isReplaying || isCpuReplay) return;
  if (moveState.isMoving) return;
  let dx = 0, dy = 0;
  if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') dy = -1;
  else if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') dy = 1;
  else if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') dx = -1;
  else if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') dx = 1;
  if (dx !== 0 || dy !== 0) {
    e.preventDefault();
    tryMoveInput(dx, dy);
  }
});