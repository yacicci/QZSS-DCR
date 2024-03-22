
# QZSS-DCRとは

ATOMS3 + ATOMIC GPS BaseでQZSSの災危通報を受信し、表示及びTFカードへの保存を行うスケッチです。


# ATOMIC GPS Baseの改造

ATOMIC GPS Base内蔵のM8030-KTは出荷時、QZSSの受信など無効になっています。
- QZSSの受信の有効化
- UBX-NAV-PVTの有効化
- UBX-RXM-SFRBXの有効化
- ボーレートを115,200bpsに変更


# QZQSMの修正

QZSS DC Report Service Decode Libraryを利用していますが、そのままではコンパイルエラーが発生します。

src/QZQSM.cpp の4484行の修正
> uint32_t sec;
> ↓
> time_t sec;


# 使い方

ATOMS3の画面をクリックするとファイルオープンとクローズをトグル操作できます。
災危通報を受信したときに画面表示とTFカードへの保存を行います。
