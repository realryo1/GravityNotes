/* GravityNote — interactions & gravity-themed touches */
(function () {
  "use strict";

  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ----- Header scroll state ----- */
  const header = document.querySelector(".site-header");
  const onScrollHeader = () => {
    if (!header) return;
    header.classList.toggle("scrolled", window.scrollY > 40);
  };
  window.addEventListener("scroll", onScrollHeader, { passive: true });
  onScrollHeader();

  /* ----- Mobile nav ----- */
  const toggle = document.getElementById("navToggle");
  const nav = document.getElementById("siteNav");
  if (toggle && nav) {
    toggle.addEventListener("click", () => {
      const open = nav.classList.toggle("open");
      toggle.classList.toggle("open", open);
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    nav.querySelectorAll("a").forEach((a) => {
      a.addEventListener("click", () => {
        nav.classList.remove("open");
        toggle.classList.remove("open");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  /* ----- Active nav link by section ----- */
  const sections = document.querySelectorAll("main section[id]");
  const navLinks = document.querySelectorAll(".site-nav a");

  const updateActiveNav = () => {
    let current = "";
    const offset = window.innerHeight * 0.35;
    sections.forEach((sec) => {
      const top = sec.getBoundingClientRect().top;
      if (top - offset < 0) current = sec.id;
    });
    navLinks.forEach((link) => {
      const href = link.getAttribute("href") || "";
      link.classList.toggle("active", href === "#" + current);
    });
  };
  window.addEventListener("scroll", updateActiveNav, { passive: true });
  updateActiveNav();

  /* ----- Scroll-in reveals ----- */
  const reveals = document.querySelectorAll(".reveal");
  if (!reduceMotion && "IntersectionObserver" in window) {
    const io = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add("visible");
            io.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.12, rootMargin: "0px 0px -40px 0px" }
    );
    reveals.forEach((el, i) => {
      el.style.transitionDelay = (i % 5) * 0.06 + "s";
      io.observe(el);
    });
  } else {
    reveals.forEach((el) => el.classList.add("visible"));
  }

  /* ----- Hero parallax ----- */
  const heroBg = document.querySelector(".hero-bg");
  if (heroBg && !reduceMotion) {
    window.addEventListener(
      "scroll",
      () => {
        const y = window.scrollY;
        if (y < window.innerHeight) {
          heroBg.style.transform = "scale(1.05) translateY(" + y * 0.25 + "px)";
        }
      },
      { passive: true }
    );
  }

  /* ----- Gravity particle field ----- */
  const field = document.getElementById("gravity-field");
  if (field && !reduceMotion) {
    const COUNT = Math.min(28, Math.floor(window.innerWidth / 40));
    const particles = [];

    // Gravity direction flips slowly over time (theme touch)
    let gravity = 1; // 1 = fall down, -1 = fall up
    let flipTimer = 0;

    for (let i = 0; i < COUNT; i++) {
      const el = document.createElement("span");
      el.className = "particle" + (i % 3 === 0 ? " pink" : "");
      field.appendChild(el);
      particles.push({
        el,
        x: Math.random() * 100,
        y: Math.random() * 100,
        size: 2 + Math.random() * 4,
        speed: 0.08 + Math.random() * 0.22,
        drift: (Math.random() - 0.5) * 0.05,
        phase: Math.random() * Math.PI * 2,
      });
      el.style.width = particles[i].size + "px";
      el.style.height = particles[i].size + "px";
    }

    let last = performance.now();
    let rafId;

    const tick = (now) => {
      const dt = Math.min(40, now - last) / 16.67;
      last = now;
      flipTimer += dt;
      // Flip gravity every ~8 seconds
      if (flipTimer > 480) {
        gravity *= -1;
        flipTimer = 0;
      }

      particles.forEach((p) => {
        p.y += p.speed * gravity * dt;
        p.x += (p.drift + Math.sin(now * 0.001 + p.phase) * 0.02) * dt;
        if (p.y > 105) p.y = -5;
        if (p.y < -5) p.y = 105;
        if (p.x > 105) p.x = -5;
        if (p.x < -5) p.x = 105;
        p.el.style.transform =
          "translate(" + p.x + "vw," + p.y + "vh)";
        p.el.style.opacity = String(0.15 + Math.abs(Math.sin(now * 0.001 + p.phase)) * 0.35);
      });

      rafId = requestAnimationFrame(tick);
    };
    rafId = requestAnimationFrame(tick);

    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        cancelAnimationFrame(rafId);
      } else {
        last = performance.now();
        rafId = requestAnimationFrame(tick);
      }
    });
  }

  /* ----- Subtle click "gravity pulse" on primary button ----- */
  document.querySelectorAll(".btn-primary").forEach((btn) => {
    btn.addEventListener("click", (e) => {
      // allow default hash navigation; add a tiny visual pulse
      btn.style.transform = "scale(0.96)";
      setTimeout(() => {
        btn.style.transform = "";
      }, 150);
    });
  });
})();
