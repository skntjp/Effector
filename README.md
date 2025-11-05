## Effector

# 概要
マイク入力にリアルタイムにエフェクト(distortion, reverb)をかける。大学の研究室体験で作った作品。

#使用ライブラリ
PortAudio; Audioの入出力を管理するのに使用。インストール先: https://www.portaudio.com/docs/v19-doxydocs/index.html

# 仕組み
1フレームごとのbufferの大きさをFRAMES_PER_BUFFERで指定している。
1フレームが満タンになる度にpatestCallback関数が実行される。
patestCallback関数内で入力にエフェクトをかけて出力する。
