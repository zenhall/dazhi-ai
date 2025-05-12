import http.client
import mimetypes
from codecs import encode

conn = http.client.HTTPSConnection("api.chatanywhere.tech")
dataList = []
boundary = 'wL36Yn8afVp8Ag7AmP8qZ0SA4n1v9T'
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=file; filename={0}'.format('output.mp3'))) 

fileType = mimetypes.guess_type('output.mp3')[0] or 'application/octet-stream' 
dataList.append(encode('Content-Type: {}'.format(fileType)))
dataList.append(encode(''))

with open('output.mp3', 'rb') as f:
   dataList.append(f.read())
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=model;'))

dataList.append(encode('Content-Type: {}'.format('text/plain')))
dataList.append(encode(''))

dataList.append(encode("whisper-1"))
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=prompt;'))

dataList.append(encode('Content-Type: {}'.format('text/plain')))
dataList.append(encode(''))

dataList.append(encode("eiusmod nulla"))
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=response_format;'))

dataList.append(encode('Content-Type: {}'.format('text/plain')))
dataList.append(encode(''))

dataList.append(encode("json"))
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=temperature;'))

dataList.append(encode('Content-Type: {}'.format('text/plain')))
dataList.append(encode(''))

dataList.append(encode("0"))
dataList.append(encode('--' + boundary))
dataList.append(encode('Content-Disposition: form-data; name=language;'))

dataList.append(encode('Content-Type: {}'.format('text/plain')))
dataList.append(encode(''))

dataList.append(encode(""))
dataList.append(encode('--'+boundary+'--'))
dataList.append(encode(''))
body = b'\r\n'.join(dataList)
payload = body
headers = {
   'Authorization': 'Bearer sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB',
   # 移除重复的 Content-Type 头（原第48行）
   'Content-type': 'multipart/form-data; boundary={}'.format(boundary)
}
try:
    conn.request("POST", "/v1/audio/transcriptions", payload, headers)
    res = conn.getresponse()
    # 输出HTTP状态码用于调试
    print(f"HTTP Status: {res.status} {res.reason}") 
    data = res.read()
    print(data.decode("utf-8"))
except Exception as e:
    print(f"Request failed: {str(e)}")