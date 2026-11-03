// Complete newly selected circuit topologies without rewriting saved family
// layouts. A generated variant belongs to its actual child structure.
const clone=value=>JSON.parse(JSON.stringify(value));
const valid=p=>Array.isArray(p?.rel_pos)&&p.rel_pos.length>=2&&p.rel_pos.every(Number.isFinite)
  &&Number.isFinite(p.rel_width)&&p.rel_width>0;
function hash(text){let h=2166136261;for(let i=0;i<text.length;i++)h=Math.imul(h^text.charCodeAt(i),16777619);return (h>>>0).toString(16);}
export function completeProfileLayouts(payload,layout,resolve){
  const components=payload.components||[],children=new Map(),repaired=[];
  layout.type_layouts||={};layout.profile_layouts||={};layout.root_layouts||={};
  for(const component of components)if(component.parentIndex!=null){
    const id=components[component.parentIndex].id;
    if(!children.has(id))children.set(id,[]);children.get(id).push(component);
  }
  const signature=kids=>kids.map(k=>k.name).sort().join('\0');
  const candidates=new Map();
  for(const entries of [layout.type_layouts,...Object.values(layout.profile_layouts)])
    for(const [type,entry] of Object.entries(entries))if(entry.children){
      const key=type.split('[config-')[0]+'\0'+Object.keys(entry.children).sort().join('\0');
      if(!candidates.has(key))candidates.set(key,[]);candidates.get(key).push(entry.children);
    }
  const aspect=component=>Math.max(Number(component.minAspectRatio)||.35,Number(component.aspectRatio)||1);
  // Children can themselves gain layout variants; finish them before a parent.
  for(const parent of [...components].reverse()){
    const kids=children.get(parent.id)||[];if(!kids.length)continue;
    const entry=resolve(parent,layout,payload.layoutKey);
    if(kids.every(child=>valid(entry.children?.[child.name])))continue;
    const pa=aspect(parent),title=Number(entry.title_bar_ratio??layout.default_settings?.title_bar_ratio??.15);
    const fits=placements=>kids.every(child=>{
      const p=placements?.[child.name];if(!valid(p))return false;
      return p.rel_pos[0]>=0&&p.rel_pos[1]*pa>=title-1e-8&&p.rel_pos[0]+p.rel_width<=1+1e-8
        &&p.rel_pos[1]*pa+p.rel_width*aspect(child)<=pa+1e-8;
    });
    const compatible=candidates.get(parent.type+'\0'+signature(kids))||[];
    let placements=compatible.find(fits);
    if(!placements){
      const average=kids.reduce((sum,child)=>sum+aspect(child),0)/kids.length;
      const cols=Math.min(kids.length,Math.max(1,Math.ceil(Math.sqrt(kids.length*average/pa))));
      const rows=Math.ceil(kids.length/cols),cellW=.84/cols,top=title+.04,cellH=Math.max(.01,pa-top-.06)/rows;
      placements=Object.fromEntries(kids.map((child,index)=>{
        const ca=aspect(child),width=Math.min(cellW*.78,cellH*.78/ca);
        return [child.name,{rel_pos:[.08+(index%cols)*cellW+(cellW-width)/2,
          (top+Math.floor(index/cols)*cellH+(cellH-width*ca)/2)/pa],rel_width:width}];
      }));
    }
    // Keep old type/configuration entries and user edits intact. Different
    // private structural/compact groups may share a public TypeName.
    const key=parent.type+'[config-fidelity-'+hash(JSON.stringify([parent.type,pa,kids.map(c=>[c.name,c.type,aspect(c)])]))+']';
    parent.layoutType=key;
    const next={...clone(entry),children:clone(placements),layout_revision:'fidelity-topology-v1'};
    if(parent.depth===0)layout.root_layouts[payload.layoutKey]=next;
    else if(parent.profileFingerprint&&parent.profileFingerprint!=='explicit'){
      layout.profile_layouts[parent.profileFingerprint]||={};layout.profile_layouts[parent.profileFingerprint][key]=next;
    }else layout.type_layouts[key]=next;
    repaired.push(parent.id);
  }
  return repaired;
}
