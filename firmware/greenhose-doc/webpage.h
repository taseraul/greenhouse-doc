inline const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>

<head>
    <style>
        /* @import url(https://fonts.googleapis.com/css?family=Roboto:300); */

        .login-page {
            width: 360px;
            padding: 8% 0 0;
            margin: auto;
        }

        .form {
            position: relative;
            z-index: 1;
            background: #FFFFFF;
            max-width: 360px;
            margin: 0 auto 100px;
            padding: 45px;
            text-align: center;
            box-shadow: 0 0 20px 0 rgba(0, 0, 0, 0.2), 0 5px 5px 0 rgba(0, 0, 0, 0.24);
        }

        .form input {
            font-family: "Roboto", sans-serif;
            outline: 0;
            background: #f2f2f2;
            width: 100%;
            border: 0;
            margin: 0 0 15px;
            padding: 15px;
            box-sizing: border-box;
            font-size: 14px;
        }

        .form button {
            font-family: "Roboto", sans-serif;
            text-transform: uppercase;
            outline: 0;
            background: #4CAF50;
            width: 100%;
            border: 0;
            padding: 15px;
            color: #FFFFFF;
            font-size: 14px;
            -webkit-transition: all 0.3 ease;
            transition: all 0.3 ease;
            cursor: pointer;
        }

        .form button:hover,
        .form button:active,
        .form button:focus {
            background: #43A047;
        }

        .form .message {
            margin: 15px 0 0;
            color: #b3b3b3;
            font-size: 12px;
        }

        .form .message a {
            color: #4CAF50;
            text-decoration: none;
        }

        .form .register-form {
            display: none;
        }

        .container {
            position: relative;
            z-index: 1;
            max-width: 300px;
            margin: 0 auto;
        }

        .container:before,
        .container:after {
            content: "";
            display: block;
            clear: both;
        }

        .container .info {
            margin: 50px auto;
            text-align: center;
        }

        .container .info h1 {
            margin: 0 0 15px;
            padding: 0;
            font-size: 36px;
            font-weight: 300;
            color: #1a1a1a;
        }

        .container .info span {
            color: #4d4d4d;
            font-size: 12px;
        }

        .container .info span a {
            color: #000000;
            text-decoration: none;
        }

        .container .info span .fa {
            color: #EF3B3A;
        }

        .on {
            background-color: #76b852;
            padding: 10%;
        }

        .off {
            padding: 10%;
        }

        body {
            background: #76b852;
            /* fallback for old browsers */
            background: rgb(141, 194, 111);
            background: linear-gradient(90deg, rgba(141, 194, 111, 1) 0%, rgba(118, 184, 82, 1) 50%);
            font-family: "Roboto", sans-serif;
            -webkit-font-smoothing: antialiased;
            -moz-osx-font-smoothing: grayscale;
        }
    </style>

</head>

<body>
    <div class="login-page">
        <div class="form">
            <!-- <form class="register-form">
                <input type="text" placeholder="name" />
                <input type="password" placeholder="password" />
                <input type="text" placeholder="email address" />
                <button>create</button>
                <p class="message">Already registered? <a href="#">Sign In</a></p>
            </form> -->
            <form class="login-form">
                <div id=wificontainer">

                    <p class="off" id="wifi1" onclick="selectWifi('wifi1')">wifi1</p>


                    <p class="on" id="wifi2" onclick="selectWifi('wifi2')">wifi2</p>

                </div>
                <input id="pw" type="password" placeholder="password" />
                <button onclick="sendCredintials()">connect</button>
                <!-- <p class="message">Not registered? <a href="#">Create an account</a></p> -->
            </form>
        </div>
    </div>
</body>
<script>
    var gateway = `ws://${window.location.hostname}/webserialws`;
    var websocket;
    var wifi;
    function sendCredintials() {
        websocket.send(String(wifi) + document.getElementById("pw").value);
    }
    function selectWifi(id) {
        wifi = document.getElementById(id).innerHTML;
        var toggle = document.getElementsByClassName("on");
        if (toggle != null) {
            toggle[0].className = "off";
        }
        document.getElementById(id).className = "on";
        console.log(wifi);
    }
    function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
    }
    function onOpen(event) {
        console.log('Connection opened');
    }

    function onClose(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }
    function onMessage(event) {
        console.log(event.data);
        if (event.data == "0") {
            document, getElementById("wificontainer").innerHTML = "";
        }
        else {
            var wifiEntry = document.createElement("p");
            wifiEntry.className = "off";
            wifiEntry.id = "wifi" + document.getElementsByName("off").length;
            wifiEntry.onclick = selectWifi(wifiEntry.id);
        }
    }
    window.addEventListener('load', onLoad);
    function onLoad(event) {
        initWebSocket();
    }
</script>

</html>)rawliteral";