
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 hang gesture recognition</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            background-color: #f4f4f9;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            padding: 16px;
        }

        .container {
            display: flex;
            flex-direction: column;
            align-items: center;
            width: 100%;
            max-width: 600px;
            background: #ffffff;
            padding: 20px;
            border-radius: 12px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
        }

        .image-frame {
            width: 100%;
            margin-bottom: 20px;
            border-radius: 8px;
            overflow: hidden;
            background-color: #eaeaea;
        }

        .image-frame img {
            display: block;
            width: 100%;
            height: auto; 
            object-fit: cover;
        }

        .button-recognize {
            width: 100%;
            max-width: 280px; 
            padding: 14px 24px;
            font-size: 18px;
            font-weight: 600;
            color: #ffffff;
            background-color: #007bff;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: background-color 0.2s ease, transform 0.1s ease;
        }
        .button-recognize:hover {
            background-color: #0056b3;
        }
        .button-recognize:active {
            transform: scale(0.98);
        }

        .button-recognize:disabled {
            color: #94a3b8;
            background-color: #e2e8f0;
            border-radius: 8px;
            cursor: default;
            transition: none;
        }
        .button-recognize:disabled:hover {
            background-color: #e2e8f0;
        }
        .button-recognize:disabled:active{
            transform: none;
        }

        .status {
            margin-top: 12px;
            font-size: 14px;
            color: #666666;
            text-align: center;
            line-height: 1.4;
            max-width: 320px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="image-frame">
            <img id="image-frame" alt="image from esp32 camera">
        </div>
        <button id="button-recognize" class="button-recognize" type="button" onclick="startRecognition()">recognize hand gesture</button>
        <p id="status" class="status">waiting for start of recognition</p>
    </div>
    <script>
        const stream = document.getElementById("image-frame");
        stream.src = "http://" + window.location.hostname + ":81/stream";
        async function startRecognition() {
            const button = document.getElementById("button-recognize");
            const status = document.getElementById("status");

            button.disabled = true;
            status.style.color = '#666666';
            status.innerText = "stay calm. recognition...";

            try {
                const response = await fetch("/recognize", {method: "POST"});

                if (!response.ok){
                    throw new Error("HTTP error " + response.status);
                }

                const data = await response.json();

                status.style.color = '#16a34a';
                status.innerText = "gesture: " + data.gesture + ", confidence: " + data.confidence + "%";
                button.disabled = false;
            }
            catch(error) {
                status.innerText = error.message;
                status.style.color = '#dc2626';
                button.disabled = false;
            }
        }
    </script>
</body>
</html>
)rawliteral";