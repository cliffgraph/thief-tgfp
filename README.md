# thief-tgfp
ThiefGriffinで出力したファイルをRaMsxMuse環境上で再生するためのLinuxコンソールアプリケーションです

## build
事前に、WiringPiをインストールしておいてください。
make を実行してください。

> make 

thief-tgfpが生成されれば成功です。

## 使い方（ファイルの再生）
RaMsxMuseが正しく動作する環境で、下記を実行します。filenameは、ThiefGriffinで出力したファイルを指定します。

> thief-tgfp -f filename

## 使い方（TgfEditorとの連携）
下記を実行します。50000は任意のポート番号に変更してもかまいませんが、TgfEditorの -ip オプションの指定と併せてください。

> thief-tgfp -udp 50000


以上
