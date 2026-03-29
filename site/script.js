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

(function () {
  var listNode = document.getElementById("release-list");
  if (!listNode) {
    return;
  }

  var downloadBtn = document.getElementById("download-latest-btn");
  var apiUrl = "https://api.github.com/repos/Zaid-maker/kernel/releases?per_page=6";

  function setState(message) {
    listNode.textContent = "";
    var state = document.createElement("li");
    state.className = "release-state";
    state.textContent = message;
    listNode.appendChild(state);
  }

  function formatDate(value) {
    if (!value) {
      return "Unknown date";
    }

    var date = new Date(value);
    return date.toLocaleDateString(undefined, {
      year: "numeric",
      month: "short",
      day: "2-digit"
    });
  }

  function findIsoAsset(release) {
    if (!release || !Array.isArray(release.assets)) {
      return null;
    }

    for (var i = 0; i < release.assets.length; ++i) {
      var asset = release.assets[i];
      if (!asset || typeof asset.name !== "string") {
        continue;
      }

      if (asset.name.toLowerCase().endsWith(".iso")) {
        return asset;
      }
    }

    return null;
  }

  fetch(apiUrl, {
    headers: {
      Accept: "application/vnd.github+json"
    }
  })
    .then(function (response) {
      if (!response.ok) {
        throw new Error("GitHub API error: " + response.status);
      }
      return response.json();
    })
    .then(function (items) {
      var releases = items.filter(function (item) {
        return item && item.draft !== true;
      });

      if (releases.length === 0) {
        setState("No public releases found yet.");
        return;
      }

      listNode.textContent = "";

      releases.slice(0, 5).forEach(function (release, index) {
        var row = document.createElement("li");
        row.className = "release-item";

        var top = document.createElement("div");
        top.className = "release-item-top";

        var tag = document.createElement("a");
        tag.className = "release-tag";
        tag.href = release.html_url;
        tag.target = "_blank";
        tag.rel = "noopener noreferrer";
        tag.textContent = release.tag_name || release.name || "untagged";

        top.appendChild(tag);

        var badge = document.createElement("span");
        badge.className = "release-badge";
        if (release.prerelease) {
          badge.textContent = "pre";
          badge.classList.add("pre");
          top.appendChild(badge);
        } else if (index === 0) {
          badge.textContent = "latest";
          top.appendChild(badge);
        }

        var meta = document.createElement("div");
        meta.className = "release-meta";

        var date = document.createElement("span");
        date.textContent = formatDate(release.published_at || release.created_at);

        var assets = document.createElement("span");
        var assetCount = Array.isArray(release.assets) ? release.assets.length : 0;
        assets.textContent = assetCount + (assetCount === 1 ? " asset" : " assets");

        meta.appendChild(date);
        meta.appendChild(assets);

        var actions = document.createElement("div");
        actions.className = "release-actions";

        var isoAsset = findIsoAsset(release);
        if (isoAsset && isoAsset.browser_download_url) {
          var isoLink = document.createElement("a");
          isoLink.className = "release-asset-link";
          isoLink.href = isoAsset.browser_download_url;
          isoLink.target = "_blank";
          isoLink.rel = "noopener noreferrer";
          isoLink.textContent = "Direct ISO";
          actions.appendChild(isoLink);
        }

        row.appendChild(top);
        row.appendChild(meta);
        if (actions.childNodes.length > 0) {
          row.appendChild(actions);
        }
        listNode.appendChild(row);

        if (index === 0 && downloadBtn) {
          if (isoAsset && isoAsset.browser_download_url) {
            downloadBtn.textContent = "Download ISO " + (release.tag_name || "Latest");
            downloadBtn.href = isoAsset.browser_download_url;
            downloadBtn.target = "_blank";
            downloadBtn.rel = "noopener noreferrer";
          } else {
            downloadBtn.textContent = "Download " + (release.tag_name || "Latest");
            downloadBtn.href = release.html_url;
            downloadBtn.target = "_blank";
            downloadBtn.rel = "noopener noreferrer";
          }
        }
      });
    })
    .catch(function () {
      setState("Unable to load releases right now. Use 'View all' to check GitHub releases.");
    });
})();
