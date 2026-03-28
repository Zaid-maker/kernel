(function () {
  var revealBlocks = document.querySelectorAll(".reveal");
  if (!("IntersectionObserver" in window) || revealBlocks.length === 0) {
    return;
  }

  var observer = new IntersectionObserver(
    function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.style.animationPlayState = "running";
          observer.unobserve(entry.target);
        }
      });
    },
    { threshold: 0.18 }
  );

  revealBlocks.forEach(function (node) {
    node.style.animationPlayState = "paused";
    observer.observe(node);
  });
})();
