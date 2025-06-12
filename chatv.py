import http.client
import json
import base64

# 读取并编码图片
with open("c:\\Users\\12393\\Desktop\\code\\zeno\\im.png", "rb") as image_file:
    encoded_image = base64.b64encode(image_file.read()).decode('utf-8')

conn = http.client.HTTPSConnection("api.chatanywhere.tech")
payload = json.dumps({
    "model": "gpt-4.1-nano",
    "messages": [
        {
            "role": "user",
            "content": [
                {"type": "text", "text": "图像里是什么"},
                {
                    "type": "image_url",
                    "image_url": {
                        "url": f"data:image/png;base64,{encoded_image}"
                    }
                }
            ]
        }
    ],
    "max_tokens": 300
})
headers = {
    'Authorization': 'Bearer sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB',
    'Content-Type': 'application/json'
}

conn.request("POST", "/v1/chat/completions", payload, headers)
res = conn.getresponse()
data = json.loads(res.read().decode("utf-8"))

# 解析响应结果
if 'choices' in data and len(data['choices']) > 0:
    answer = data['choices'][0]['message']['content'].strip().lower()
    print(answer)
else:
    print("识别失败，响应数据：", data)