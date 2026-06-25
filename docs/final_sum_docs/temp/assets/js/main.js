/* ==========================================================================
   MyChat Docs — main.js
   导航 / 滚动监听 / reveal 触发 / 代码复制 / 移动端菜单
   ========================================================================== */
(function () {
  'use strict';

  document.addEventListener('DOMContentLoaded', init);

  function init() {
    initNav();
    initReveal();
    initCopy();
    initMobileMenu();
    initTOC();
  }

  /* —— 顶部导航：滚动加底边 —— */
  function initNav() {
    var nav = document.querySelector('.nav');
    if (!nav) return;
    var onScroll = function () {
      if (window.scrollY > 8) nav.classList.add('scrolled');
      else nav.classList.remove('scrolled');
    };
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
  }

  /* —— reveal：IntersectionObserver 触发显现 —— */
  function initReveal() {
    var els = document.querySelectorAll('.reveal');
    if (!els.length) return;
    if (!('IntersectionObserver' in window)) {
      els.forEach(function (el) { el.classList.add('in'); });
      return;
    }
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) {
          e.target.classList.add('in');
          io.unobserve(e.target);
        }
      });
    }, { threshold: 0.12, rootMargin: '0px 0px -8% 0px' });
    els.forEach(function (el) { io.observe(el); });
  }

  /* —— 代码块复制 —— */
  function initCopy() {
    document.querySelectorAll('.code-copy').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var block = btn.closest('.code-block');
        var code = block && block.querySelector('pre code');
        if (!code) return;
        var text = code.textContent;
        var done = function () {
          btn.classList.add('copied');
          var orig = btn.textContent;
          btn.textContent = '已复制 ✓';
          setTimeout(function () {
            btn.classList.remove('copied');
            btn.textContent = orig;
          }, 1600);
        };
        if (navigator.clipboard && navigator.clipboard.writeText) {
          navigator.clipboard.writeText(text).then(done, fallback);
        } else { fallback(); }
        function fallback() {
          var ta = document.createElement('textarea');
          ta.value = text; document.body.appendChild(ta); ta.select();
          try { document.execCommand('copy'); done(); } catch (e) {}
          document.body.removeChild(ta);
        }
      });
    });
  }

  /* —— 移动端菜单 —— */
  function initMobileMenu() {
    var toggle = document.querySelector('.nav-toggle');
    var links = document.querySelector('.nav-links');
    if (!toggle || !links) return;
    toggle.addEventListener('click', function () { links.classList.toggle('open'); });
    links.querySelectorAll('a').forEach(function (a) {
      a.addEventListener('click', function () { links.classList.remove('open'); });
    });
  }

  /* —— TOC 高亮：滚动章节激活对应目录项 —— */
  function initTOC() {
    var toc = document.querySelector('.toc');
    if (!toc) return;
    var links = Array.prototype.slice.call(toc.querySelectorAll('a'));
    var map = {};
    links.forEach(function (l) {
      var id = l.getAttribute('href');
      if (id && id.charAt(0) === '#') {
        var target = document.querySelector(id);
        if (target) map[id] = l;
      }
    });
    var ids = Object.keys(map);
    if (!ids.length) return;
    if (!('IntersectionObserver' in window)) return;
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        var link = map['#' + e.target.id];
        if (!link) return;
        if (e.isIntersecting) {
          links.forEach(function (l) { l.classList.remove('active'); });
          link.classList.add('active');
        }
      });
    }, { rootMargin: '-20% 0px -70% 0px', threshold: 0 });
    ids.forEach(function (id) {
      var el = document.querySelector(id);
      if (el) io.observe(el);
    });
  }

  /* —— 阅读进度条 + 返回顶部 —— */
  function initScrollUI() {
    var bar = document.querySelector('.read-progress');
    var btn = document.querySelector('.back-top');
    if (!bar && !btn) return;
    var onScroll = function () {
      var st = window.scrollY;
      var docH = document.documentElement.scrollHeight - window.innerHeight;
      var pct = docH > 0 ? (st / docH) * 100 : 0;
      if (bar) bar.style.width = pct + '%';
      if (btn) {
        if (st > window.innerHeight * 0.6) btn.classList.add('show');
        else btn.classList.remove('show');
      }
    };
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
    if (btn) btn.addEventListener('click', function () {
      window.scrollTo({ top: 0, behavior: 'smooth' });
    });
  }
})();
