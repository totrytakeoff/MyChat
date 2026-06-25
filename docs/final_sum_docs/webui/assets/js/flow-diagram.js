/* ==========================================================================
   MyChat Docs — flow-diagram.js
   链路流光核心组件：自动布局节点 + SVG 连线 + 粒子流动 + 节点高亮 + 说明切换
   用法：在 .flow 容器内放 <div class="flow-stage" data-flow='{...}'></div>
   数据格式：
   {
     "nodes": [
       {"id":"web","label":"Web Client","group":"client"},
       {"id":"gw","label":"Gateway","group":"gateway"},
       ...
     ],
     "edges": [["web","gw"],["gw","user"],...],
     "notes": {"web":"<title>Web Client</title>客户端只访问 Gateway，不直连业务服务"}
   }
   ========================================================================== */
(function () {
  'use strict';

  var GROUP_COLOR = {
    client: '#3D5A6C', gateway: '#D97757', service: '#6B8E6B',
    store: '#C99A52', push: '#9B7BA8', external: '#A39B92'
  };

  document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('[data-flow]').forEach(initFlow);
  });

  function initFlow(stage) {
    var raw;
    try { raw = JSON.parse(stage.getAttribute('data-flow')); }
    catch (e) { return; }
    if (!raw || !raw.nodes) return;
    var data = raw;

    /* —— 布局：按 group 分层，自动计算坐标 —— */
    var groups = data.layout || ['client', 'gateway', 'service', 'store'];
    var W = stage.clientWidth || 760;
    var padX = 24, padY = 30;
    var nodeW = 130, nodeH = 46, gapY = 90;
    var layerCount = groups.length;
    var innerW = W - padX * 2;
    var xStep = innerW / (layerCount - 1 || 1);

    var pos = {};
    groups.forEach(function (g, gi) {
      var layerNodes = data.nodes.filter(function (n) { return n.group === g; });
      var n = layerNodes.length || 1;
      var totalH = (n - 1) * gapY;
      var startY = padY + 40;
      layerNodes.forEach(function (node, ni) {
        pos[node.id] = {
          x: padX + xStep * gi - nodeW / 2,
          y: startY + (ni - (n - 1) / 2) * gapY,
          node: node
        };
      });
    });

    var maxY = 0, maxX = 0;
    Object.keys(pos).forEach(function (k) {
      maxY = Math.max(maxY, pos[k].y + nodeH);
      maxX = Math.max(maxX, pos[k].x + nodeW);
    });
    var H = maxY + padY;
    var vbW = maxX + padX;

    /* —— 构建 SVG —— */
    var svgNS = 'http://www.w3.org/2000/svg';
    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('class', 'flow-svg');
    svg.setAttribute('viewBox', '0 0 ' + vbW + ' ' + H);
    svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');

    var defs = document.createElementNS(svgNS, 'defs');
    defs.innerHTML =
      '<marker id="arrow-' + rand() + '" class="flow-arrow" viewBox="0 0 10 10" refX="9" refY="5" ' +
      'markerWidth="7" markerHeight="7" orient="auto-start-reverse">' +
      '<path d="M0,0 L10,5 L0,10 z" fill="#A39B92"/></marker>';
    svg.appendChild(defs);

    /* —— 连线 —— */
    var edgePaths = [];
    (data.edges || []).forEach(function (edge) {
      var a = pos[edge[0]], b = pos[edge[1]];
      if (!a || !b) return;
      var ax = a.x + nodeW, ay = a.y + nodeH / 2;
      var bx = b.x, by = b.y + nodeH / 2;
      var mx = (ax + bx) / 2;
      var d = 'M' + ax + ',' + ay + ' C' + mx + ',' + ay + ' ' + mx + ',' + by + ' ' + bx + ',' + by;

      var base = document.createElementNS(svgNS, 'path');
      base.setAttribute('d', d);
      base.setAttribute('class', 'flow-line');
      svg.appendChild(base);

      var glow = document.createElementNS(svgNS, 'path');
      glow.setAttribute('d', d);
      glow.setAttribute('class', 'flow-line-glow');
      glow.style.opacity = '0';
      glow.style.strokeDasharray = '6 8';
      glow.style.strokeDashoffset = '0';
      svg.appendChild(glow);
      edgePaths.push({ glow: glow, base: base, from: edge[0], to: edge[1] });
    });

    /* —— 节点 —— */
    var nodeEls = {};
    data.nodes.forEach(function (node) {
      var p = pos[node.id];
      if (!p) return; /* 跳过未分到位置的节点 */
      var g = document.createElementNS(svgNS, 'g');
      g.setAttribute('class', 'flow-node');
      g.setAttribute('transform', 'translate(' + p.x + ',' + p.y + ')');
      g.setAttribute('data-id', node.id);

      var rect = document.createElementNS(svgNS, 'rect');
      rect.setAttribute('width', nodeW);
      rect.setAttribute('height', nodeH);
      rect.setAttribute('rx', 10);
      rect.setAttribute('fill', '#FAF7F2');
      rect.setAttribute('stroke', GROUP_COLOR[node.group] || '#E2DBD0');
      rect.setAttribute('stroke-width', '1.8');
      g.appendChild(rect);

      var text = document.createElementNS(svgNS, 'text');
      text.setAttribute('x', nodeW / 2);
      text.setAttribute('y', nodeH / 2 + 4);
      text.setAttribute('text-anchor', 'middle');
      text.setAttribute('class', 'flow-label');
      text.style.fontWeight = '600';
      text.style.fontSize = '12px';
      text.style.fill = '#1A1714';
      text.textContent = node.label;
      g.appendChild(text);

      g.addEventListener('click', function () { selectNode(node.id); });
      g.addEventListener('mouseenter', function () { highlightEdge(node.id, true); });
      g.addEventListener('mouseleave', function () { highlightEdge(node.id, false); });

      svg.appendChild(g);
      nodeEls[node.id] = g;
    });

    stage.appendChild(svg);

    /* —— 粒子流动动画 —— */
    var particles = [];
    var rafId = null, t0 = null, playing = true;
    edgePaths.forEach(function (ep) {
      var c = document.createElementNS(svgNS, 'circle');
      c.setAttribute('r', '3.5');
      c.setAttribute('class', 'flow-particle');
      c.style.opacity = '0';
      svg.appendChild(c);
      particles.push({ edge: ep, circle: c, progress: Math.random() });
    });

    function animate(ts) {
      if (t0 === null) t0 = ts;
      var dt = (ts - t0) / 1000; t0 = ts;
      if (!playing) { rafId = null; return; }
      particles.forEach(function (p) {
        p.progress += dt * 0.45;
        if (p.progress > 1) p.progress -= 1;
        var len = p.edge.base.getTotalLength();
        var pt = p.edge.base.getPointAtLength(len * p.progress);
        p.circle.setAttribute('cx', pt.x);
        p.circle.setAttribute('cy', pt.y);
        p.circle.style.opacity = 0.9;
      });
      rafId = requestAnimationFrame(animate);
    }
    rafId = requestAnimationFrame(animate);

    /* —— 暂停/播放控制 —— */
    var flowRoot = stage.closest('.flow');
    var controls = flowRoot ? flowRoot.querySelector('.flow-controls') : null;
    if (controls) {
      controls.querySelectorAll('.flow-btn').forEach(function (btn) {
        if (btn.dataset.action === 'toggle') {
          btn.addEventListener('click', function () {
            playing = !playing;
            if (playing) { t0 = null; rafId = requestAnimationFrame(animate); btn.textContent = '暂停流光'; btn.classList.add('active'); }
            else { if (rafId) cancelAnimationFrame(rafId); btn.textContent = '播放流光'; btn.classList.remove('active'); }
          });
        }
        if (btn.dataset.action === 'replay') {
          btn.addEventListener('click', function () {
            particles.forEach(function (p) { p.progress = 0; });
            if (!playing) { playing = true; t0 = null; rafId = requestAnimationFrame(animate); }
          });
        }
      });
    }

    /* —— 节点选中 + 说明 —— */
    var note = flowRoot ? flowRoot.querySelector('.flow-note') : null;
    var current = null;
    function selectNode(id) {
      current = id;
      Object.keys(nodeEls).forEach(function (k) {
        nodeEls[k].classList.toggle('active', k === id);
      });
      highlightEdge(id, true, true);
      if (note && data.notes && data.notes[id]) {
        note.style.opacity = '0';
        setTimeout(function () {
          note.innerHTML = data.notes[id];
          note.style.opacity = '1';
        }, 150);
      }
    }

    function highlightEdge(id, on, lock) {
      edgePaths.forEach(function (ep) {
        if (ep.from === id || ep.to === id) {
          ep.glow.style.opacity = on ? '0.9' : '0';
          ep.glow.style.transition = 'opacity .25s';
        } else if (!lock) {
          ep.glow.style.opacity = '0';
        }
      });
    }

    /* —— 默认选中第一个节点 —— */
    if (data.nodes[0]) selectNode(data.nodes[0].id);

    /* —— 响应式重布局 —— */
    var ro;
    if (window.ResizeObserver) {
      ro = new ResizeObserver(function () { /* SVG viewBox 自适应，无需重建 */ });
      ro.observe(stage);
    }
  }

  function rand() { return Math.random().toString(36).slice(2, 8); }
})();
