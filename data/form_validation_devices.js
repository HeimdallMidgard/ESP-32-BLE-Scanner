const form = document.getElementById('form')
const errorElement = document.getElementById('error')

const device_name1 = document.getElementById('device_name1')
const device_name2 = document.getElementById('device_name2')
const device_name3 = document.getElementById('device_name3')
const uuid1 = document.getElementById('uuid1')
const uuid2 = document.getElementById('uuid2')
const uuid3 = document.getElementById('uuid3')


form.addEventListener('submit', (e) => {

    let messages = []


if (device_name1.value.indexOf('\"') > -1 || device_name1.value.indexOf("\'") > -1)
{
  messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (device_name2.value.indexOf('\"') > -1 || device_name2.value.indexOf("\'") > -1)
{
    messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (device_name3.value.indexOf('\"') > -1 || device_name3.value.indexOf("\'") > -1)
{
    messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (uuid1.value.indexOf('\"') > -1 || uuid1.value.indexOf("\'") > -1)
{
    messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (uuid2.value.indexOf('\"') > -1 || uuid2.value.indexOf("\'") > -1)
{
    messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (uuid3.value.indexOf('\"') > -1 || uuid3.value.indexOf("\'") > -1)
{
    messages.push('Error You are using not allowed characters like: \" or \' ')
}

if (messages.length > 0) {
    e.preventDefault()
    errorElement.innerText = messages.join(', ')
  }
})