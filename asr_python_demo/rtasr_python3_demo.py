# -*- encoding:utf-8 -*-
import hashlib
import hmac
import base64
from socket import *
import json, time, threading
from websocket import create_connection
import websocket
from urllib.parse import quote
import logging
import pyaudio
import os
import datetime
import sys

# reload(sys)
# sys.setdefaultencoding("utf8")
class Client():
    def __init__(self):
        base_url = "ws://rtasr.xfyun.cn/v1/ws"
        ts = str(int(time.time()))
        tt = (app_id + ts).encode('utf-8')
        md5 = hashlib.md5()
        md5.update(tt)
        baseString = md5.hexdigest()
        baseString = bytes(baseString, encoding='utf-8')

        apiKey = api_key.encode('utf-8')
        signa = hmac.new(apiKey, baseString, hashlib.sha1).digest()
        signa = base64.b64encode(signa)
        signa = str(signa, 'utf-8')
        self.end_tag = "{\"end\": true}"

        self.ws = create_connection(base_url + "?appid=" + app_id + "&ts=" + ts + "&signa=" + quote(signa))
        self.trecv = threading.Thread(target=self.recv)
        self.trecv.start()

    def send_from_mic(self):
        # PyAudio配置
        CHUNK = 1280  # 每次读取的块大小
        FORMAT = pyaudio.paInt16  # 16位格式
        CHANNELS = 1  # 单声道
        RATE = 16000  # 采样率16kHz
        
        p = pyaudio.PyAudio()
        
        # 打开麦克风流
        stream = p.open(format=FORMAT,
                       channels=CHANNELS,
                       rate=RATE,
                       input=True,
                       frames_per_buffer=CHUNK)
        
        print("开始录音，按Ctrl+C停止...")
        
        try:
            while True:
                # 读取音频数据
                data = stream.read(CHUNK)
                # 发送到服务器
                self.ws.send(data)
                time.sleep(0.04)
        except KeyboardInterrupt:
            print("录音停止")
        finally:
            # 停止并关闭流
            stream.stop_stream()
            stream.close()
            p.terminate()
            
            # 发送结束标签
            self.ws.send(bytes(self.end_tag.encode('utf-8')))
            print("发送结束标签成功")

    def extract_text_from_json(self, json_data):
        """从讯飞实时语音识别的JSON中提取文本内容"""
        try:
            if "data" not in json_data:
                return None
                
            data = json_data["data"]
            if not data:
                return None
                
            try:
                # 尝试解析内部的JSON文本
                cn_data = json.loads(data)
                if "cn" not in cn_data:
                    return None
                    
                st_data = cn_data["cn"].get("st")
                if not st_data:
                    return None
                    
                rt_list = st_data.get("rt", [])
                if not rt_list:
                    return None
                    
                text = ""
                for rt in rt_list:
                    ws_list = rt.get("ws", [])
                    for ws in ws_list:
                        cw_list = ws.get("cw", [])
                        for cw in cw_list:
                            w = cw.get("w", "")
                            text += w
                            
                return text.strip()
                
            except (json.JSONDecodeError, KeyError, TypeError):
                # 如果解析内部JSON失败，直接返回data
                return data.strip() if isinstance(data, str) else None
                
        except Exception as e:
            print(f"解析JSON错误: {e}")
            return None

    def recv(self):
        # 存储已完成的句子
        completed_sentences = []
        
        # 当前正在处理的句子
        current_sentence = ""
        
        # 上次接收到结果的时间
        last_result_time = time.time()
        
        # 沉默超时时间（毫秒），超过这个时间没有新结果则认为句子结束
        silence_timeout = 0.6  # 300毫秒
        
        # 清屏函数
        def clear_screen():
            os.system('cls' if os.name == 'nt' else 'clear')
            
        # 显示所有识别结果
        def display_results(sentence_completed=False):
            clear_screen()
            print("=" * 60)
            print("实时语音识别结果")
            print("=" * 60)
            print("\n[按Ctrl+C停止录音]\n")
            
            # 显示已完成的句子
            if completed_sentences:
                print("-" * 60)
                print("【已完成的句子】")
                for i, sentence in enumerate(completed_sentences, 1):
                    print(f"{i}. {sentence}")
                print("-" * 60)
            
            # 如果刚刚完成一句话，显示提示
            if sentence_completed:
                print("\n【检测到一句话已说完!】\n")
            
            # 显示当前正在说的句子
            if current_sentence:
                print("\n【当前识别中】")
                print(current_sentence)
            
            # 刷新输出
            sys.stdout.flush()
            
        # 检查句子是否超时的线程
        def check_timeout():
            nonlocal current_sentence
            
            while True:
                # 当前时间
                current_time = time.time()
                
                # 如果有当前正在处理的句子，检查是否超时
                if current_sentence and (current_time - last_result_time > silence_timeout):
                    # 句子已超时，认为一句话已结束
                    if current_sentence.strip() and current_sentence not in completed_sentences:
                        completed_sentences.append(current_sentence)
                        display_results(True)  # 显示句子已完成的提示
                        current_sentence = ""
                
                # 暂停一段时间再检查
                time.sleep(0.1)
        
        # 启动超时检查线程
        timeout_thread = threading.Thread(target=check_timeout)
        timeout_thread.daemon = True  # 设为守护线程，主线程结束时自动终止
        timeout_thread.start()
            
        try:
            while self.ws.connected:
                result = str(self.ws.recv())
                if len(result) == 0:
                    print("\n识别结束")
                    break
                
                # 更新最后接收时间
                last_result_time = time.time()
                    
                result_dict = json.loads(result)
                
                # 解析结果
                if result_dict["action"] == "started":
                    clear_screen()
                    print("=" * 60)
                    print("连接成功，开始识别...")
                    print("=" * 60)
                    print("\n[按Ctrl+C停止录音]\n")

                elif result_dict["action"] == "result":
                    # 提取识别文本
                    text = self.extract_text_from_json(result_dict)
                    
                    if text and text.strip():
                        # 更新当前句子
                        current_sentence = text
                        
                        # 显示结果
                        display_results()

                elif result_dict["action"] == "error":
                    print("\n识别错误")
                    self.ws.close()
                    return
        except websocket.WebSocketConnectionClosedException:
            print("\n连接已关闭")

    def close(self):
        self.ws.close()
        print("连接已关闭")


if __name__ == '__main__':
    logging.basicConfig()

    app_id = "872c31f2"
    api_key = "d54282953514c40782355312881174be"

    client = Client()
    client.send_from_mic()
