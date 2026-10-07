const miis = [
  { name: "Luca", nickname: "Luca", avatar: "luca" },
  { name: "Sora", nickname: "Sora", avatar: "sora" },
  { name: "Mika", nickname: "Mika", avatar: "mika" },
  { name: "Rex", nickname: "Rex", avatar: "rex" },
  { name: "Nina", nickname: "Nina", avatar: "nina" },
  { name: "Tobi", nickname: "Tobi", avatar: "tobi" },
];

let selectedIndex = 1;
const list = document.querySelector("#mii-list");
const miiName = document.querySelector("#mii-name");
const nicknameInput = document.querySelector("#nickname");
const mainMii = document.querySelector("#main-mii");
const toast = document.querySelector("#toast");

function avatarMarkup(type) {
  return `<div class="avatar avatar-${type}" aria-hidden="true">
    <i class="hair"></i><i class="face"></i><i class="eye left"></i><i class="eye right"></i><i class="smile"></i>
  </div>`;
}

function renderList() {
  list.innerHTML = miis.map((mii, index) => `
    <button class="mii-row ${index === selectedIndex ? "selected" : ""}" type="button" role="option"
      aria-selected="${index === selectedIndex}" data-index="${index}">
      ${avatarMarkup(mii.avatar)}<strong>${mii.name}</strong>
    </button>
  `).join("");
}

function selectMii(index) {
  selectedIndex = index;
  const mii = miis[index];
  miiName.value = mii.name;
  nicknameInput.value = mii.nickname;
  mainMii.className = `mii-figure avatar-${mii.avatar}`;
  mainMii.setAttribute("aria-label", mii.name);
  renderList();
}

function showToast(message) {
  toast.textContent = message;
  toast.classList.add("visible");
  window.clearTimeout(showToast.timeout);
  showToast.timeout = window.setTimeout(() => toast.classList.remove("visible"), 2600);
}

list.addEventListener("click", (event) => {
  const button = event.target.closest(".mii-row");
  if (button) selectMii(Number(button.dataset.index));
});

document.querySelector("#editor-form").addEventListener("submit", (event) => {
  event.preventDefault();
  showToast("Mockup: Changes are not saved.");
});

document.querySelector("#cancel-button").addEventListener("click", () => {
  showToast("Mockup: There are no changes to discard.");
});

document.querySelector("#settings-button").addEventListener("click", () => showToast("Settings are not available in this mockup."));

document.querySelectorAll("[data-window-action]").forEach((button) => {
  button.addEventListener("click", () => showToast(`${button.getAttribute("aria-label")} – demo window action`));
});

const renameOverlay = document.querySelector("#rename-overlay");

function openRenameMockup() {
  renameOverlay.classList.add("is-open");
  renameOverlay.setAttribute("aria-hidden", "false");
  document.querySelector(".keyboard-tab.selected").focus();
}

function closeRenameMockup() {
  renameOverlay.classList.remove("is-open");
  renameOverlay.setAttribute("aria-hidden", "true");
}

nicknameInput.addEventListener("click", openRenameMockup);
nicknameInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter" || event.key === " ") {
    event.preventDefault();
    openRenameMockup();
  }
});

document.querySelectorAll("[data-modal-close]").forEach((button) => {
  button.addEventListener("click", closeRenameMockup);
});

renameOverlay.addEventListener("click", (event) => {
  if (event.target === renameOverlay) closeRenameMockup();
});

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && renameOverlay.classList.contains("is-open")) closeRenameMockup();
});

document.querySelectorAll(".keyboard-grid button").forEach((key) => {
  key.addEventListener("click", () => {
    window.clearTimeout(key.pressTimer);
    key.classList.add("is-pressed");
    key.pressTimer = window.setTimeout(() => key.classList.remove("is-pressed"), 170);
  });
});

document.querySelectorAll(".keyboard-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".keyboard-tab").forEach((item) => item.classList.toggle("selected", item === tab));
  });
});

selectMii(selectedIndex);
