#ifndef SPIDER_IMPL_H_
#define SPIDER_IMPL_H_

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <webgpu/webgpu.h>

#include <cglm/cglm.h>

#define SP_ASSERT(assertion) assert(assertion)
#define SP_MALLOC(size) malloc(size)
#define SP_ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])

#define _SP_RELEASE_RESOURCE(Type, Name) if(Name) {wgpu##Type##Release(Name); Name = NULL;}
#define _SP_GET_DEFAULT_IF_ZERO(value, default_value) value ? value : default_value

#define SP_INVALID_ID (0)

#define REF(Type_Name) Type_Name[static 1]
#define CONST_REF(Type_Name) const Type_Name[static 1]



#endif // SPIDER_IMPL_H_