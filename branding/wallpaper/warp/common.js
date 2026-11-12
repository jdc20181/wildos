const W_PATH='M37 17 L50.34 40 L64.75 64.84 L79.16 40 L92.5 17 L105.84 40 L134.26 89 L120.76 112.28 L92.5 63.55 L64.91 111.12 L23.66 40Z M148.5 17 L162 40 L148.5 63 L135 40Z';
function rng(seed){return ()=>{seed|=0;seed=seed+0x6D2B79F5|0;let t=Math.imul(seed^seed>>>15,1|seed);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296}}
function hue(h,s=100,l=60,a=1){return `hsla(${h},${s}%,${l}%,${a})`}
function neonW(x,cx,cy,scale,col){
  const P=new Path2D(W_PATH);
  x.save();x.translate(cx-92.83*scale,cy-64.64*scale);x.scale(scale,scale);
  x.fillStyle='rgba(0,0,0,0.85)';x.fill(P);
  x.lineJoin='round';
  for(const [b,w,a] of [[60,4.5,0.55],[26,2.6,0.9],[8,1.4,1],[0,0.6,1]]){
    x.shadowColor=col;x.shadowBlur=b;x.lineWidth=w/scale*3.2;
    x.strokeStyle=b?col:'rgba(235,255,240,0.95)';x.globalAlpha=a;x.stroke(P);}
  x.restore();x.globalAlpha=1;x.shadowBlur=0;
}
