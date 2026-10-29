#include "ai_provider.h"
#include "ai_json.h"
#include <string.h>
extern const ai_model_adapter_t ai_ark_adapter;
const ai_model_adapter_t *ai_model_adapter(const char *id){return id&&!strcmp(id,ai_ark_adapter.id)?&ai_ark_adapter:NULL;}
