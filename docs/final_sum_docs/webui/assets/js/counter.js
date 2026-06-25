/* ==========================================================================
   MyChat Docs — counter.js
   数字滚动计数（用于压测指标）。data-count="714.4" data-decimals="1"
   ========================================================================== */
(function () {
  'use strict';
  document.addEventListener('DOMContentLoaded', function () {
    var els = document.querySelectorAll('[data-count]');
    if (!els.length) return;
    if (!('IntersectionObserver' in window)) {
      els.forEach(finalize); return;
    }
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { run(e.target); io.unobserve(e.target); }
      });
    }, { threshold: 0.4 });
    els.forEach(function (el) { io.observe(el); });
  });

  function run(el) {
    var target = parseFloat(el.getAttribute('data-count')) || 0;
    var decimals = parseInt(el.getAttribute('data-decimals') || '0', 10);
    var dur = parseFloat(el.getAttribute('data-dur') || '1400');
    var unit = el.getAttribute('data-unit') || '';
    var start = 0, t0 = null;
    el.classList.add('counting');
    function step(ts) {
      if (t0 === null) t0 = ts;
      var p = Math.min((ts - t0) / dur, 1);
      var eased = 1 - Math.pow(1 - p, 3);
      var val = start + (target - start) * eased;
      render(el, val, decimals, unit);
      if (p < 1) requestAnimationFrame(step);
      else { render(el, target, decimals, unit); setTimeout(function () { el.classList.remove('counting'); }, 1200); }
    }
    requestAnimationFrame(step);
  }

  function render(el, val, decimals, unit) {
    var txt = decimals > 0 ? val.toFixed(decimals) : Math.round(val).toString();
    el.innerHTML = txt + (unit ? '<span class="unit">' + unit + '</span>' : '');
  }

  function finalize(el) {
    var target = parseFloat(el.getAttribute('data-count')) || 0;
    var decimals = parseInt(el.getAttribute('data-decimals') || '0', 10);
    var unit = el.getAttribute('data-unit') || '';
    render(el, target, decimals, unit);
  }
})();
