// Included inside TriggeredGrants; owns only the stock descriptor extension.
inline std::string Text(Obj* value){auto* text=value?Bridge::ObjectToString(value):nullptr;return text?text->str():std::string{};}
inline void Properties(Obj* object,Obj* properties){
    if(!object || object->klass!=descriptorType.GetClass() || !properties)return;
    Obj* duration=Bridge::DictionaryValue(properties,"Duration");Obj* operation=Bridge::DictionaryValue(properties,"ModifyOperation");
    if(!duration && !operation)return;
    Options value;value.enabled=true;
    const auto d=Text(duration),op=Text(operation);
    value.duration=duration?Engine::EffectDurations::Parse(d):0;
    if(value.duration<0){LOG_ERR("CustomComponents/TriggeredGrant: unsupported Duration=%s",d.c_str());value.enabled=false;}
    if(!operation)value.operation=0;
    else if(op=="Set" || op=="1")value.operation=1;
    else if(op=="Add" || op=="2")value.operation=2;
    else {LOG_ERR("CustomComponents/TriggeredGrant: unsupported ModifyOperation=%s",op.c_str());value.enabled=false;}
    descriptors.Set(object,value,value.enabled);
    auto remove=BNM::Class(properties).GetMethod("Remove",1).cast<bool>();
    if(remove.IsValid())for(auto key:{"Duration","ModifyOperation"})Core::Metadata::Bind(remove,properties)(BNM::CreateMonoString(key));
}
inline void (*originalDescriptorCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void (*originalDescriptorPool)(Obj*,Obj*,const void*)=nullptr;
inline void (*originalRuntimeCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void (*originalRuntimePool)(Obj*,Obj*,const void*)=nullptr;
inline Obj* (*originalConvert)(Obj*,Obj*,Obj*,const void*)=nullptr;
static void DescriptorCopy(Obj* self,Obj* source,Obj* instances,const void* method){const auto value=descriptors.Get(source);originalDescriptorCopy(self,source,instances,method);descriptors.Set(self,value,value.enabled);}
static void DescriptorPool(Obj* self,Obj* instances,const void* method){descriptors.Set(self,{},false);originalDescriptorPool(self,instances,method);}
static void RuntimeCopy(Obj* self,Obj* source,Obj* instances,const void* method){const auto value=effects.Get(source);originalRuntimeCopy(self,source,instances,method);effects.Set(self,value,value.enabled);}
static void RuntimePool(Obj* self,Obj* instances,const void* method){effects.Set(self,{},false);originalRuntimePool(self,instances,method);}
static Obj* Convert(Obj* self,Obj* instances,Obj* entity,const void* method){const auto value=descriptors.Get(self);Obj* result=originalConvert(self,instances,entity,method);effects.Set(result,value,value.enabled);return result;}
inline bool InitializeSerialization(){
    return Bridge::beforeProperties.Add(Properties) &&
        Core::Hooks::Install(descriptorType.GetMethod("DeepCopyFrom",2),DescriptorCopy,originalDescriptorCopy) &&
        Core::Hooks::Install(descriptorType.GetMethod("HandleReturnToPool",1),DescriptorPool,originalDescriptorPool) &&
        Core::Hooks::Install(descriptorType.GetMethod("ToEffect",2),Convert,originalConvert) &&
        Core::Hooks::Install(runtimeType.GetMethod("DeepCopyFrom",2),RuntimeCopy,originalRuntimeCopy) &&
        Core::Hooks::Install(runtimeType.GetMethod("HandleReturnToPool",1),RuntimePool,originalRuntimePool);
}