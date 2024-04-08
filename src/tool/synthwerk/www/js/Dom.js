/* Dom.js
 * Conveniences for working with HTML Dom nodes.
 */
 
import { Injector } from "./Injector.js";

export class Dom {
  static getDependencies() {
    return [Injector, Document, Window];
  }
  constructor(injector, document, window) {
    this.injector = injector;
    this.document = document;
    this.window = window;
    
    const mutationObserver = new window.MutationObserver(e => this.onMutation(e));
    mutationObserver.observe(document.body, { subtree: true, childList: true });
    
    this.ignoredRemoval = [];
  }
  
  /* Create an element and append it to some parent element.
   * args:
   *  - Array: CSS class names.
   *  - Object: HTML attributes or "on_EVENTNAME":function.
   *  - Anything else: innerText.
   */
  spawn(parent, tagName, ...args) {
    const child = this.document.createElement(tagName);
    for (const arg of args) {
      if (arg instanceof Array) {
        for (const clsname of arg) child.classList.add(clsname);
      } else if (arg && (typeof(arg) === "object")) {
        for (const k of Object.keys(arg)) {
          if (k.startsWith("on_")) child.addEventListener(k.substring(3), arg[k]);
          else child.setAttribute(k, arg[k]);
        }
      } else {
        child.innerText = arg;
      }
    }
    parent.appendChild(child);
    return child;
  }
  
  /* Instantiate a view controller class, attach its element to this parent,
   * and return the new controller instance.
   */
  spawnController(parent, cls, overrides) {
    if (!overrides) overrides = [];
    const element = this.spawn(parent, this.tagNameForControllerClass(cls));
    const controller = this.injector.instantiate(cls, [...overrides, element]);
    element.__controller = controller;
    element.classList.add(cls.name);
    return controller;
  }
  
  /* Call before reparenting an element, to suppress onRemovedFromDom for it and its descendants.
   */
  ignoreNextRemoval(element) {
    this.ignoredRemoval.push(element);
  }
  
  presentModal(controllerClass) {
    let dialog = this.spawn(this.document.body, "DIALOG");
    const controller = this.spawnController(dialog, controllerClass);
    let ignoreNextClick = false;
    dialog.addEventListener("mousedown", event => {
      ignoreNextClick = this.eventInDialog(dialog, event);
    });
    dialog.addEventListener("click", event => {
      if (!ignoreNextClick && !this.eventInDialog(dialog, event)) {
        dialog.close();
      }
      ignoreNextClick = false;
    });
    dialog.showModal();
    return controller;
  }
  
  eventInDialog(dialog, event) {
    const bounds = dialog.getBoundingClientRect();
    if (event.clientX < bounds.x) return false;
    if (event.clientY < bounds.y) return false;
    if (event.clientX >= bounds.x + bounds.width) return false;
    if (event.clientY >= bounds.y + bounds.height) return false;
    return true;
  }
  
  dismissModalForController(controller) {
    const dialog = controller.element?.parentNode;
    if (!dialog || (dialog.tagName !== "DIALOG")) return;
    dialog.close();
  }
  
  // Window.prompt(), but as a Promise.
  // Cancelling rejects the Promise.
  prompt(text, initial) {
    return new Promise((resolve, reject) => {
      const result = this.window.prompt(text, initial);
      if (typeof(result) === "string") resolve(result);
      else reject();
    });
  }
  
  tagNameForControllerClass(cls) {
    const deps = cls.getDependencies?.() || [];
    for (const depcls of deps) {
      if ((depcls === HTMLElement) || HTMLElement.isPrototypeOf(depcls)) {
        return this.tagNameForElementClassName(depcls.name);
      }
    }
    return "DIV";
  }
  
  tagNameForElementClassName(clsname) {
    switch (clsname) {
      case "HTMLCanvasElement": return "CANVAS";
      // Lots of others, and the class name does not always match the tag name exactly.
      // Add as we encounter needs for them. Most controllers will be fine with plain old "DIV".
    }
    return "DIV";
  }
  
  onMutation(events) {
    for (const event of events) {
      if (!event.removedNodes) continue;
      for (const node of event.removedNodes) {
        this.notifyRemovedNodes(node);
      }
    }
  }
  
  notifyRemovedNodes(parent) {
    const ignorep = this.ignoredRemoval.indexOf(parent);
    if (ignorep >= 0) {
      this.ignoredRemoval.splice(ignorep, 1);
      return;
    }
    if (parent.children) {
      for (const child of parent.children) {
        this.notifyRemovedNodes(child);
      }
    }
    if (parent.__controller) {
      parent.__controller.onRemoveFromDom?.();
      parent.__controller.element = null;
      parent.__controller = null;
    }
  }
}

Dom.singleton = true;
