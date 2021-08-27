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
        state_message = "Socket connected";
        state = false;
      };
      socket.onerror = function(e) {
        state_message = `Socket error: ${e.message}`;
        state = false;
      }
    },
  };
}

function getDevices() {
  return {
    devices: null,
    isLoading: false,
    init() {
      this.isLoading = true;
      fetch(`/api/devices`)
        .then((res) => res.json())
        .then((data) => {
          this.devices = data;
          this.isLoading = false;
        });
    },
    save() {
      const el = this;
      this.devices = this.devices.filter((d) => d.name !== "" && d.uuid !== "");
      const data = new URLSearchParams();
      data.append("devices", JSON.stringify(this.devices));
      fetch(`/api/devices`, {
        method: "post",
        headers: {
          "Content-type": "application/x-www-form-urlencoded",
        },
        body: data,
      }).then(function (res) {
        message = "Devices saved, rebooting. Click ok to refresh the page";
        if (res.status !== 200) {
          message =
            "Something went wrong saving devices. Check the logs and click ok to refresh the page";
        }
        el.$refs.modalText.textContent = message;
        el.$refs.modalCheckbox.checked = true;
      });
    },
  };
}
