const form = document.getElementById('form')
const errorElement = document.getElementById('error')

const ssid = document.getElementById('ssid')
const password = document.getElementById('password')
const room = document.getElementById('room')
const mqttServer = document.getElementById('mqttServer')
const mqttPort = document.getElementById('mqttPort')
const mqttUser = document.getElementById('mqttUser')
const mqttPassword = document.getElementById('mqttPassword')


form.addEventListener('submit', (e) => {

    let messages = []

    if (ssid.value === '' || ssid.value == null) {
      messages.push('Error: SSID is required')
    }

    if (room.value === '' || room.value == null) {
      messages.push('Error: Room is required')
    }

    if (password.value === '' || password.value == null) {
        messages.push('Error: Wifi Password is required')
    }

    if (mqttServer.value === '' || mqttServer.value == null) {
        messages.push('Error: MQTT Server is required')
    }
      
    if (mqttPort.value === '' || mqttPort.value == null) {
      messages.push('Error: MQTT Port is required')
    }

    if (room.value.indexOf('\"') > -1 || room.value.indexOf("\'") > -1)
    {
      messages.push('Error: You are using not allowed characters like: \" or \' ')
    }

    if (mqttServer.value.indexOf('\"') > -1 || mqttServer.value.indexOf("\'") > -1)
    {
      messages.push('Error: You are using not allowed characters like: \" or \' ')
    }


    if (messages.length > 0) {
        e.preventDefault()
        errorElement.innerText = messages.join(', ')
      }
})