/* ==========================================================================
   MyChat Docs — mermaid-init.js
   Mermaid 主题定制（匹配 Claude 暖色风格）。需在 mermaid CDN 之后加载。
   ========================================================================== */
(function () {
  'use strict';
  function boot() {
    if (!window.mermaid) { setTimeout(boot, 50); return; }
    window.mermaid.initialize({
      startOnLoad: false,
      theme: 'base',
      themeVariables: {
        background: '#F5F1EB',
        primaryColor: '#FAF7F2',
        primaryTextColor: '#1A1714',
        primaryBorderColor: '#D97757',
        lineColor: '#A39B92',
        secondaryColor: '#EDE7DE',
        tertiaryColor: '#F5F1EB',
        fontFamily: '-apple-system, "PingFang SC", "Microsoft YaHei", sans-serif',
        fontSize: '14px',
        nodeBorder: '#E2DBD0',
        clusterBkg: '#EDE7DE',
        clusterBorder: '#E2DBD0',
        edgeLabelBackground: '#F5F1EB',
        actorBkg: '#F4E3D8',
        actorBorder: '#D97757',
        actorTextColor: '#1A1714',
        actorLineColor: '#D97757',
        signalColor: '#3D3833',
        signalTextColor: '#1A1714',
        labelBoxBkgColor: '#FAF7F2',
        labelBoxBorderColor: '#D97757',
        labelTextColor: '#1A1714',
        sequenceNumberColor: '#FFFFFF'
      },
      flowchart: { curve: 'basis', padding: 16, htmlLabels: true },
      sequence: { actorMargin: 60, boxMargin: 10, mirrorActors: false },
      securityLevel: 'loose'
    });
    /* 渲染所有 .mermaid 块 */
    var blocks = document.querySelectorAll('.mermaid');
    blocks.forEach(function (b, i) {
      if (b.getAttribute('data-processed')) return;
      try {
        window.mermaid.run({ nodes: [b] });
      } catch (e) {
        b.setAttribute('data-processed', 'true');
      }
    });
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else { boot(); }
})();
