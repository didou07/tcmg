import http.client, base64, sys, urllib.parse, time
H="127.0.0.1"; P=18080; fails=[]
def ok(n,c,x=""):
    print(("PASS " if c else "FAIL ")+n+((" -> "+str(x)) if not c and x!="" else ""));
    if not c: fails.append(n)
def req(m,path,body=None,h=None):
    c=http.client.HTTPConnection(H,P,timeout=10); hh={"Connection":"close"}; hh.update(h or {})
    if isinstance(body,dict): body=urllib.parse.urlencode(body); hh["Content-Type"]="application/x-www-form-urlencoded"
    c.request(m,path,body=body,headers=hh); r=c.getresponse(); d=r.read().decode("utf-8","replace")
    hd={}
    for k,v in r.getheaders(): hd.setdefault(k.lower(),[]).append(v)
    c.close(); return r.status,hd,d
b64=lambda u,p: "Basic "+base64.b64encode((u+":"+p).encode()).decode()

st,hd,_=req("GET","/users"); ok("unauthenticated -> redirect to login",st==302 and "/login" in hd["location"][0],(st,hd.get("location")))
st,hd,_=req("POST","/login",{"u":"admin","p":"secret"})
ck=hd.get("set-cookie",[""])[0]
ok("login sets cookie",st==302 and "tcmg_session=" in ck,(st,ck))
ok("cookie is HttpOnly + SameSite=Strict",("HttpOnly" in ck) and ("SameSite=Strict" in ck),ck)
ok("cookie lives 24h (86400), not 1h",("Max-Age=86400" in ck),ck)
tok=ck.split(";")[0]
st,_,b=req("GET","/users",h={"Cookie":tok}); ok("cookie grants access",st==200)
st,_,b=req("GET","/users",h={"cookie":tok}); ok("lowercase 'cookie' header accepted",st==200,st)
st,_,b=req("GET","/users",h={"Authorization":b64("admin","secret")}); ok("Basic auth works",st==200,st)
# Basic auth brute force must now feed the fail-ban
res=[]
for i in range(12):
    st,_,_=req("GET","/users",h={"Authorization":b64("admin","wrong%d"%i)}); res.append(st)
ok("wrong Basic credentials never succeed",all(s in (302,401) for s in res),res)
st,_,b=req("GET","/users",h={"Authorization":b64("admin","secret")})
ok("Basic auth from a banned IP is refused (was a full ban bypass)",st!=200,st)
st,_,b=req("POST","/login",{"u":"admin","p":"secret"})
ok("login from banned IP -> 429 with clear message",st==429 and "temporarily blocked" in b,(st,b[:80]))
st,hd,b=req("GET","/failban",h={"Cookie":tok}); ok("failban page lists the ban (session cookie still valid)",st==200 and "127.0.0.1" in b,st)
st,_,_=req("GET","/failban?action=clear&ip=127.0.0.1",h={"Cookie":tok})
st,_,b=req("GET","/failban",h={"Cookie":tok}); ok("ban cleared",("127.0.0.1" not in b.split("<tbody")[-1]) if "<tbody" in b else True)
# after clearing, ONE typo must not re-ban at once (fails reset)
req("POST","/login",{"u":"admin","p":"typo"})
st,_,b=req("POST","/login",{"u":"admin","p":"secret"}); ok("one typo after unban does not re-ban (fails reset)",st==302,st)
print("\nFAILED:",len(fails),fails); sys.exit(1 if fails else 0)
