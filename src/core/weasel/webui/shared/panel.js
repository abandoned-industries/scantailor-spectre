// ============================================================
// Panel framework — reusable helpers for web-based option panels.
// Handles QWebChannel setup, value syncing, and common widget
// wiring so individual panels can be declarative.
// ============================================================

const Panel = (function() {
  let bridge = null;
  let panelBase = null;
  let syncing = false;
  let remeasureTimer = null;
  const setters = {};

  function scheduleRemeasure() {
    if (!panelBase || !panelBase.remeasure) return;
    if (remeasureTimer) clearTimeout(remeasureTimer);
    // Defer past the current tick so the DOM has reflowed before we measure.
    remeasureTimer = setTimeout(function() { panelBase.remeasure(); }, 30);
  }

  /**
   * Register a slider. Options:
   *   format:     (raw) => string            — how to display the value
   *   toBridge:   (raw) => number            — convert slider value to bridge value
   *   fromBridge: (bridge) => number         — convert bridge value to slider value
   *   inputToRaw: (displayed) => rawNumber   — convert user-typed display value back to slider raw
   *                                            (defaults to identity; used when value field is an <input>)
   */
  function setupSlider(id, opts) {
    opts = opts || {};
    const slider = document.getElementById(id);
    const valueEl = document.getElementById(id + '-val');
    const valueIsInput = valueEl && valueEl.tagName === 'INPUT';
    const formatter = opts.format || function(v) { return String(Math.round(v)); };
    const toBridge = opts.toBridge || function(v) { return v; };
    const fromBridge = opts.fromBridge || function(v) { return v; };
    const inputToRaw = opts.inputToRaw || function(v) { return v; };

    function writeValueEl(raw) {
      if (!valueEl) return;
      const text = formatter(raw);
      if (valueIsInput) valueEl.value = text;
      else valueEl.textContent = text;
    }

    slider.addEventListener('input', function() {
      const raw = parseFloat(slider.value);
      writeValueEl(raw);
      if (bridge && !syncing && bridge.setValue) {
        bridge.setValue(id, toBridge(raw));
      }
    });

    if (valueIsInput) {
      const commit = function() {
        if (syncing) return;
        const displayVal = parseFloat(valueEl.value);
        if (isNaN(displayVal)) return;
        let raw = inputToRaw(displayVal);
        const min = parseFloat(slider.min);
        const max = parseFloat(slider.max);
        if (!isNaN(min) && raw < min) raw = min;
        if (!isNaN(max) && raw > max) raw = max;
        slider.value = raw;
        writeValueEl(parseFloat(slider.value));  // re-read in case slider snapped to step
        if (bridge && bridge.setValue) {
          bridge.setValue(id, toBridge(parseFloat(slider.value)));
        }
      };
      valueEl.addEventListener('change', commit);
      valueEl.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') { e.preventDefault(); valueEl.blur(); }
      });
    }

    setters[id] = function(bridgeValue) {
      syncing = true;
      slider.value = fromBridge(bridgeValue);
      writeValueEl(parseFloat(slider.value));
      syncing = false;
    };
  }

  function setupCheckbox(id) {
    const cb = document.getElementById(id);
    cb.addEventListener('change', function() {
      if (bridge && !syncing && bridge.setCheck) {
        bridge.setCheck(id, cb.checked);
      }
    });
    setters[id] = function(checked) {
      syncing = true;
      cb.checked = !!checked;
      syncing = false;
    };
  }

  function setupRadio(name, ids) {
    ids.forEach(function(id) {
      const rb = document.getElementById(id);
      rb.addEventListener('change', function() {
        if (rb.checked && bridge && !syncing && bridge.setRadio) {
          bridge.setRadio(name, id);
        }
      });
    });
    setters[name] = function(selectedId) {
      syncing = true;
      ids.forEach(function(id) {
        const rb = document.getElementById(id);
        if (rb) rb.checked = (id === selectedId);
      });
      syncing = false;
    };
  }

  function setupButton(id, action) {
    const btn = document.getElementById(id);
    btn.addEventListener('click', function() {
      if (bridge && bridge.action) {
        bridge.action(action || id);
      }
    });
  }

  function setupSelect(id) {
    const sel = document.getElementById(id);
    sel.addEventListener('change', function() {
      if (bridge && !syncing && bridge.setSelect) {
        bridge.setSelect(id, sel.value);
      }
    });
    setters[id] = function(value) {
      syncing = true;
      sel.value = value;
      syncing = false;
    };
  }

  function setupVisibility(id, displayValue) {
    const el = document.getElementById(id);
    const visibleDisplay = displayValue || '';
    setters[id] = function(visible) {
      syncing = true;
      const next = visible ? visibleDisplay : 'none';
      if (el.style.display !== next) {
        el.style.display = next;
        scheduleRemeasure();
      }
      syncing = false;
    };
  }

  /**
   * Initialize the panel. Call after all setupSlider/setupCheckbox/etc.
   * Connects to the C++ bridge and requests initial values.
   */
  function init(onReady) {
    new QWebChannel(qt.webChannelTransport, function(channel) {
      bridge = channel.objects.bridge;
      if (bridge.valuesChanged) {
        bridge.valuesChanged.connect(function(values) {
          Object.keys(values).forEach(function(key) {
            if (setters[key]) setters[key](values[key]);
          });
        });
      }
      panelBase = channel.objects.panelBase || null;
      if (onReady) onReady(bridge);
      if (bridge.requestValues) bridge.requestValues();

      // Belt-and-suspenders: ResizeObserver catches anything that changes
      // document height outside of a visibility setter (font reflow, etc.).
      if (panelBase && panelBase.remeasure && typeof ResizeObserver === 'function') {
        var observer = new ResizeObserver(scheduleRemeasure);
        observer.observe(document.documentElement);
        observer.observe(document.body);
      }
    });
  }

  return {
    setupSlider: setupSlider,
    setupCheckbox: setupCheckbox,
    setupRadio: setupRadio,
    setupButton: setupButton,
    setupSelect: setupSelect,
    setupVisibility: setupVisibility,
    init: init,
    getBridge: function() { return bridge; }
  };
})();
