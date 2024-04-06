import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { RootUi } from "./js/RootUi.js";

window.addEventListener("load", () => {
  const injector = new Injector(window, document);
  const dom = injector.instantiate(Dom);
  document.body.innerHTML = "";
  const root = dom.spawnController(document.body, RootUi);
}, { once: true });
