setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    var x = document.getElementById("logs").value;
    document.getElementById("logs").value = this.responseText + x;
  };
  xhttp.open("GET", "/send_scan_results", true);
  xhttp.send();

  var yhttp = new XMLHttpRequest();
  yhttp.onreadystatechange = function () {
    var y = document.getElementById("logs").value;
    document.getElementById("logs").value = this.responseText + y;
  };
  yhttp.open("GET", "/send_logs", true);
  yhttp.send();
}, 1500);
