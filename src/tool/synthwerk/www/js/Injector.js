/* Injector.js
 * Generic dependency injection.
 */
 
export class Injector {
  static getDependencies() { // Won't be used; just setting an example.
    return [Window, Document];
  }
  constructor(window, document) {
    this.window = window;
    this.document = document;
    
    this.singletons = {
      Window: window,
      Document: document,
      Injector: this,
    };
    this.inProgress = [];
    this.nextDiscriminator = 1;
  }
  
  instantiate(cls, overrides) {
    if (cls === "discriminator") return this.nextDiscriminator++;
    const cname = cls.name;
    if (!cname) throw new Error(`Class does not have a valid name.`);
    let instance = this.singletons[cname];
    if (instance) return instance;
    if (this.inProgress.indexOf(cname) >= 0) {
      throw new Error(`Dependency cycle involving: ${JSON.stringify(this.inProgress)}`);
    }
    this.inProgress.push(cname);
    try {
      const depClasses = cls.getDependencies?.() || [];
      const deps = [];
      for (const dcls of depClasses) {
        const dinst = overrides?.find(o => ((typeof(dcls) === "function") && (o instanceof dcls))) || this.instantiate(dcls, overrides);
        deps.push(dinst);
      }
      instance = new cls(...deps);
    } finally {
      const ix = this.inProgress.indexOf(cname);
      if (ix >= 0) this.inProgress.splice(ix, 1);
    }
    if (cls.singleton) this.singletons[cname] = instance;
    return instance;
  }
}

Injector.singleton = true; // Won't be used; just setting an example.
