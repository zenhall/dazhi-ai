import http.client
import json
import base64

# 读取并编码图片
with open("c:\\Users\\12393\\Desktop\\code\\zeno\\im.png", "rb") as image_file:
    encoded_image = base64.b64encode(image_file.read()).decode('utf-8')

# 构建payload
payload_dict = {
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
}

payload = json.dumps(payload_dict, indent=2)

print("=== Python程序发送的JSON结构 ===")
print(payload)
print("\n=== JSON结构分析 ===")
print("messages[0].role:", payload_dict["messages"][0]["role"])
print("messages[0].content长度:", len(payload_dict["messages"][0]["content"]))
print("messages[0].content[0].type:", payload_dict["messages"][0]["content"][0]["type"])
print("messages[0].content[0].text:", payload_dict["messages"][0]["content"][0]["text"])
print("messages[0].content[1].type:", payload_dict["messages"][0]["content"][1]["type"])
print("messages[0].content[1].image_url.url前缀:", payload_dict["messages"][0]["content"][1]["image_url"]["url"][:50] + "...")

headers = {
    'Authorization': 'Bearer sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB',
    'Content-Type': 'application/json'
}

conn = http.client.HTTPSConnection("api.chatanywhere.tech")
conn.request("POST", "/v1/chat/completions", payload, headers)
res = conn.getresponse()
data = json.loads(res.read().decode("utf-8"))

print("\n=== 服务器响应 ===")
print("状态码:", res.status)
print("响应数据:", json.dumps(data, indent=2, ensure_ascii=False))

# 解析响应结果
if 'choices' in data and len(data['choices']) > 0:
    answer = data['choices'][0]['message']['content'].strip().lower()
    print("\n=== 最终结果 ===")
    print(answer)
else:
    print("识别失败，响应数据：", data)
