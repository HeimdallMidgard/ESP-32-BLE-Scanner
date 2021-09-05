function get_input_type(value) {
  input = {
    column: {
      classes: "col-9 ignore-screen level-item",
    },
  };
  switch (typeof value) {
    case "number":
      input.type = "number";
      input.style = "width: 100%";
      break;
    case "boolean":
      input["type"] = "checkbox";
      input["style"] = "";
      input["column"]["classes"] += " form-ext-control";
      break;
    case "string":
      input.type = "text";
      input.style = "width: 100%";
      break;
    default:
      console.log("Default reached");
  }
  return input;
}

async function save_data(url, params) {
  const data = new URLSearchParams();
  Object.entries(params).forEach((param) => {
    const [key, val] = param;
    data.append(key, val);
  });
  return await fetch(url, {
    method: "post",
    headers: {
      "Content-type": "application/x-www-form-urlencoded",
    },
    body: data,
  });
}

async function show_snackbar(el, page, res) {
  if (res.status === 200) {
    el.innerText = `${page} successfully saved`;
    el.classList.add("success");
  } else {
    el.innerText = `${page} could not be successfully saved, check logs`;
    el.classList.add("error");
  }
  el.classList.add("show");
  setTimeout(function () {
    el.classList.remove("show");
    el.classList.add("success");
    el.classList.add("error");
  }, 2000);
}

function load() {
  return {
    tab: 'logging',
    $refs: null,
    logging: {
      wrap_lines: false,
    },
    logging: load_logging(),
    devices: load_devices(),
    settings: load_settings(),
    isLoaded: false,
    async init() {
      this.logging.start_socket();
      await this.devices.refresh();
      await this.settings.refresh();
      this.isLoaded = true;
    },
    async get_partial(partial) {
      url = `/partials/${partial}.html`;
      res = await fetch(url);
      text = await res.text();
      return text;
    },
    toggle_menu() {
      this.$refs.menu_button.classList.toggle("active");
      this.$refs.menu.classList.toggle("active");
    },
    set_active_tab(tab) {
      this.tab = tab;
      this.toggle_menu();
    },
    get_tab_classes(tab) {
      return (
        "col-xs-12 col-md-10 col-lg-8 offset-center p-0" +
        (this.tab == tab ? "" : " u-none")
      );
    },
    async reboot() {
      await fetch("/api/reboot");
      while (true) {
        await fetch("/api/status");
        if (res.status === 200) {
          break;
        }
      }
      location.reload();
    },
    async clearIgnores() {
      fetch("/api/clearignores");
    },
  };
}

function load_settings() {
  return {
    url: "/api/settings",
    data: {},
    $refs: null,
    tab: "network",
    dirty: false,
    async refresh() {
      res = await fetch(this.url);
      data = await res.json();
      this.data = data;
    },
    async save() {
      params = { settings: JSON.stringify(this.data) };
      res = await save_data(this.url, params);
      if (res.status === 200) {
        this.dirty = false;
      }
      show_snackbar(this.$refs.snackbar, "Settings", res);
    },
    get_input_type(value) {
      return get_input_type(value);
    },
  };
}

function load_devices() {
  return {
    url: "/api/devices",
    data: [],
    editting: {
      name: "",
      type: "",
      uuid: "",
      index: null,
    },
    modal: null,
    $refs: null,
    tab: "all",
    types: [],
    inputs: {
      uuid: {
        pattern: "",
        title: ""
      }
    },
    async refresh() {
      res = await fetch(this.url);
      data = await res.json();
      this.types = [...new Set(data.map((d) => d["type"]))];
      this.data = data;
    },
    async save() {
      params = { devices: JSON.stringify(this.data) };
      res = await save_data(this.url, params);
      show_snackbar(this.$refs.snackbar, "Devices", res);
      this.refresh();
    },
    add() {
      this.editting.index = this.data.length;
      this.$refs.device_modal.classList.add("shown");
    },
    async delete(i) {
      this.data.splice(i, 1);
      await this.save();
    },
    edit(i) {
      device = JSON.parse(JSON.stringify(this.data[i]));
      device.index = i;
      this.editting = device;
      this.$refs.device_modal.classList.add("shown");
    },
    set_id_pattern(type) {
      switch (type) {
        case 'Beacon':
          this.inputs.uuid.pattern = '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}';
          this.inputs.uuid.title = 'Uuid should be in the format abcdef01-2345-6789-abcd-ef0123456789';
          break;
        default:
          this.inputs.uuid.pattern = ".*"
          this.inputs.uuid.title = ""
      }
    },
    reset_editting() {
      this.$refs.device_modal.classList.remove("shown");
      this.editting = {
        name: "",
        type: "",
        uuid: "",
        index: null,
      };
    },
    save_editting() {
      new_dev = this.editting;
      i = new_dev.index;
      delete new_dev.index;
      this.data[i] = new_dev;
      this.reset_editting();
      this.save();
    },
    validate_editting() {
      e = this.editting;
      return e.name.length > 0 && e.uuid.length > 0;
    },
  };
}

function load_logging() {
  return {
    log_lines: [],
    state: false,
    state_message: "Socket disconnected",
    async start_socket() {
      log_lines = this.log_lines;
      state = this.state;
      state_message = this.state_message;
      let socket = new WebSocket(`ws://${window.location.hostname}/ws`);
      socket.onopen = function (e) {
        state = true;
        state_message = "Socket connected";
      };
      socket.onmessage = function (e) {
        log_lines.unshift(e.data);
      };
      socket.onclose = function (e) {
        state_message = "Socket closed";
        state = false;
      };
      socket.onerror = function (e) {
        state_message = `Socket error: ${e.message}`;
        state = false;
      };
    },
  };
}
