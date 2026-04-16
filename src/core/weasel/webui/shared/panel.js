// ============================================================
// Panel framework — reusable helpers for web-based option panels.
// Handles QWebChannel setup, value syncing, and common widget
// wiring so individual panels can be declarative.
// ============================================================

const Panel = (function() {
  let bridge = null;
  let syncing = false;
  const setters = {};

  /**
   * Register a slider. Options:
   *   format:    (raw) => string            — how to display the value
   *   toBridge:  (raw) => number            — convert slider value to bridge value
   *   fromBridge: (bridge) => number        — convert bridge value to slider value
   */
  function setupSlider(id, opts) {
    opts = opts || {};
    const slider = document.getElementById(id);
    const valueEl = document.getElementById(id + '-val');
    const formatter = opts.format || function(v) { return String(Math.round(v)); };
    const toBridge = opts.toBridge || function(v) { return v; };
    const fromBridge = opts.fromBridge || function(v) { return v; };

    slider.addEventListener('input', function() {
      const raw = parseFloat(slider.value);
      if (valueEl) valueEl.textContent = formatter(raw);
      if (bridge && !syncing && bridge.setValue) {
        bridge.setValue(id, toBridge(raw));
      }
    });

    setters[id] = function(bridgeValue) {
      syncing = true;
      slider.value = fromBridge(bridgeValue);
      if (valueEl) valueEl.textContent = formatter(parseFloat(slider.value));
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
      el.style.display = visible ? visibleDisplay : 'none';
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
      if (onReady) onReady(bridge);
      if (bridge.requestValues) bridge.requestValues();
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
