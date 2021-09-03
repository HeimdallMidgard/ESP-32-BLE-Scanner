function getInputType(value) {
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

async function saveData(url, params) {
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

async function showSnackbar(el, page, res) {
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

function loadData() {
  return {
    tab: "logging",
    $refs: null,
    logging: {
      wrap_lines: false,
    },
    settings: {
      $refs: null,
      url: "/api/settings",
      data: {},
      tab: "all",
      async refresh() {
        res = await fetch(this.url);
        data = await res.json();
        this.data = data;
      },
      async save() {
        params = { settings: JSON.stringify(this.data) };
        res = await saveData(this.url, params);
        showSnackbar(this.$refs.snackbar, "Settings", res);
        this.refresh();
      },
      getInputType(value) {
        return getInputType(value);
      },
    },
    devices: {
      $refs: null,
      url: "/api/devices",
      data: {},
      editting: {
        name: "",
        type: "",
        uuid: "",
        index: null,
      },
      modal: null,
      tab: "all",
      types: [],
      async refresh() {
        res = await fetch(this.url);
        data = await res.json();
        this.types = [...new Set(data.map((d) => d["type"]))];
        this.data = data;
      },
      async save() {
        params = { devices: JSON.stringify(this.data) };
        res = await saveData(this.url, params);
        showSnackbar(this.$refs.snackbar, "Devices", res);
        this.refresh();
      },
      add() {
        this.editting.index = this.data.length;
        this.modal.classList.add("shown");
      },
      delete(i) {
        this.data.splice(i, 1);
      },
      edit(i) {
        device = JSON.parse(JSON.stringify(this.data[i]));
        device.index = i;
        this.editting = device;
        this.modal.classList.add("shown");
      },
      resetEditting() {
        this.modal.classList.remove("shown");
        this.editting = {
          name: "",
          type: "",
          uuid: "",
          index: null,
        };
      },
      saveEditting() {
        i = this.editting.index;
        delete this.editting.index;
        this.data[i] = this.editting;
        this.resetEditting();
        this.save();
      },
      validateEditting() {
        e = this.editting;
        return e.name.length > 0 && e.uuid.length > 0;
      },
    },
    isLoaded: false,
    async getPartial(partial) {
      url = `/partials/${partial}.html`;
      res = await fetch(url);
      text = await res.text();
      return text;
    },
    propagate(refs) {
      this.$refs = refs;
      this.devices.$refs = refs;
      this.settings.$refs = refs;
    },
    toggleMenu() {
      this.$refs.menu_button.classList.toggle("active");
      this.$refs.menu.classList.toggle("active");
    },
    setActiveTab(tab) {
      this.tab = tab;
      this.toggleMenu();
    },
    getTabClasses(tab) {
      return (
        "col-xs-12 col-md-10 col-lg-8 offset-center p-0" +
        (this.tab == tab ? "" : " u-none")
      );
    },
    async init() {
      await this.settings.refresh();
      await this.devices.refresh();
      this.isLoaded = true;
    },
  };
}

function startSocket() {
  return {
    log_lines: [],
    state: false,
    state_message: "Socket disconnected",
    init() {
      log_lines =  this.log_lines;
      state = this.state;
      state_message = this.state_message;
      let socket = new WebSocket(`ws://${window.location.hostname}/ws`);
      socket.onopen = function(e) {
        state = true;
        state_message = "Socket connected";
      };
      socket.onmessage = function(e) {
        log_lines.unshift(e.data);
      };
      socket.onclose = function(e) {
        state_message = "Socket closed";
        state = false;
      };
      socket.onerror = function(e) {
        state_message = `Socket error: ${e.message}`;
        state = false;
      }
    },
  };
}
