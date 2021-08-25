function getSettings() {
  return {
    settings: null,
    options: {
      ui: {
        style: {
          options: ["default", "dark", "nord"]
        }
      },
    },
    isLoading: false,
    init() {
      this.isLoading = true;
      fetch(`/api/settings`)
        .then((res) => res.json())
        .then((data) => {
          this.settings = data;
          this.isLoading = false;
        });
    },
    save() {
      const data = new URLSearchParams();
      data.append('settings', JSON.stringify(this.settings))
      fetch(`/api/settings`, {
        method: 'post',
        headers: {
          "Content-type": "application/x-www-form-urlencoded",
        },
        body: data
      }).then(function(res) {
        message = "Settings saved. Press ok to reboot"
        if (res.status !== 200) {
          message = "Something went wrong saving settings. Check logs"
        }
      })
    }
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
      this.devices = this.devices.filter(d => d.name !== '' && d.uuid !== '');
      const data = new URLSearchParams();
      data.append('devices', JSON.stringify(this.devices))
      fetch(`/api/devices`, {
        method: 'post',
        headers: {
          "Content-type": "application/x-www-form-urlencoded",
        },
        body: data
      }).then(function(res) {
        message = "Devices saved"
        if (res.status !== 200) {
          message = "Something went wrong saving devices. Check logs"
        }
      })
    }
  };
}