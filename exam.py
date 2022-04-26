def fun(num, ids, rem):
    h={}
    for i in ids:
        try:
            h[i]+=1
        except:
            h[i]=1
    h={k:v for k,v in sorted(h.items(), key=lambda item:item[1])}
    temp=[]
    for key in h.keys():
            temp+=[key]*h[key]
    print(temp[rem:])
    return len(set(temp[rem:]))
print(fun(6,[1,1,1,2,3,2],2))