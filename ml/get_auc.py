# -*- coding: utf-8 -*-
import sys
import numpy as np


def getAuc(labels, pred) :

    sorted_pred = sorted(range(len(pred)), key = lambda i : pred[i])
    pos = 0.0 #正样本个�??
    neg = 0.0 #负样本个�??
    auc = 0.0 
    last_pre = pred[sorted_pred[0]]
    count = 0.0
    pre_sum = 0.0
    pos_count = 0.0  #记录预测值相等的样本中标签是正的样本的个�??
    for i in range(len(sorted_pred)) :
        if labels[sorted_pred[i]] > 0:
            pos += 1        
        else:
            neg += 1       
        if last_pre != pred[sorted_pred[i]]: #当前的预测概率�??�与前�0�0??个�??�不相同
            auc += pos_count * pre_sum / count  
            count = 1          
            pre_sum = i + 1     #更新为当前的rank    
            last_pre = pred[sorted_pred[i]] 
            if labels[sorted_pred[i]] > 0:
                pos_count = 1   #如果当前样本是正样本 ，则置为1
            else:
                pos_count = 0   #反之置为0
        else:
            pre_sum += i + 1    #记录rank的和
            count += 1          #记录rank和对应的样本数，pre_sum / count就是平均值了
            if labels[sorted_pred[i]] > 0:#如果是正样本
                pos_count += 1  #正样本数�??1
    auc += pos_count * pre_sum / count #加上�??后一个预测�??�相同的样本�??
    auc -= pos *(pos + 1) / 2 #减去正样本在正样本之前的情况
    auc = auc / (pos * neg)  #除以总的组合�??
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
