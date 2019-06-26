# -*- coding: utf-8 -*-
import sys
import numpy as np


def getAuc(labels, pred) :

    sorted_pred = sorted(range(len(pred)), key = lambda i : pred[i])
    pos = 0.0 #姝ｆ牱鏈釜鏁??
    neg = 0.0 #璐熸牱鏈釜鏁??
    auc = 0.0 
    last_pre = pred[sorted_pred[0]]
    count = 0.0
    pre_sum = 0.0
    pos_count = 0.0  #璁板綍棰勬祴鍊肩浉绛夌殑鏍锋湰涓爣绛炬槸姝ｇ殑鏍锋湰鐨勪釜鏁??
    for i in range(len(sorted_pred)) :
        if labels[sorted_pred[i]] > 0:
            pos += 1        
        else:
            neg += 1       
        if last_pre != pred[sorted_pred[i]]: #褰撳墠鐨勯娴嬫鐜囧??间笌鍓嶄¸??涓??间笉鐩稿悓
            auc += pos_count * pre_sum / count  
            count = 1          
            pre_sum = i + 1     #鏇存柊涓哄綋鍓嶇殑rank    
            last_pre = pred[sorted_pred[i]] 
            if labels[sorted_pred[i]] > 0:
                pos_count = 1   #濡傛灉褰撳墠鏍锋湰鏄鏍锋湰 锛屽垯缃负1
            else:
                pos_count = 0   #鍙嶄箣缃负0
        else:
            pre_sum += i + 1    #璁板綍rank鐨勫拰
            count += 1          #璁板綍rank鍜屽搴旂殑鏍锋湰鏁帮紝pre_sum / count灏辨槸骞冲潎鍊间簡
            if labels[sorted_pred[i]] > 0:#濡傛灉鏄鏍锋湰
                pos_count += 1  #姝ｆ牱鏈暟鍔??1
    auc += pos_count * pre_sum / count #鍔犱笂鏈??鍚庝竴涓娴嬪??肩浉鍚岀殑鏍锋湰缁??
    auc -= pos *(pos + 1) / 2 #鍑忓幓姝ｆ牱鏈湪姝ｆ牱鏈箣鍓嶇殑鎯呭喌
    auc = auc / (pos * neg)  #闄や互鎬荤殑缁勫悎鏁??
    return auc

if __name__ == "__main__":
    #y = [1,     0,  0,   1,   1,  1,  0,  1,  1,  1]
    #pred = [0.9, 0.9,0.8, 0.8, 0.7,0.7,0.7,0.6,0.5,0.4]
    y = []
    pred = []
    for line in sys.stdin:
        line = line.strip()
        line_split = line.split('\t')
        pred.append(float(line_split[0]))
        y.append(int(line_split[1]))
    y =np.array(y)
    pred = np.array(pred)
    #fpr, tpr, thresholds = metrics.roc_curve(y, pred, pos_label=1)
    #print(metrics.auc(fpr, tpr))
    print(getAuc(y, pred))
