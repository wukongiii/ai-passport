#include "ai_provider.h"
#include <string.h>
extern const ai_model_adapter_t ai_compatible_adapter;
const ai_model_adapter_t *ai_model_adapter(const char *id){return id&&!strcmp(id,ai_compatible_adapter.id)?&ai_compatible_adapter:NULL;}
