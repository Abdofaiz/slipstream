# Slipstream Protocol Extension System

## نظرة عامة

تم تطوير نظام مرن لإضافة دعم البروتوكولات الإضافية إلى slipstream. هذا النظام يسمح بإضافة بروتوكولات جديدة بسهولة دون تعديل الكود الأساسي.

## البروتوكولات المدعومة

### 1. TCP (الافتراضي)
- **الاستخدام**: نفق TCP تقليدي
- **الميزات**: اتصال موثوق، دعم multiplexing
- **المنفذ الافتراضي**: 80

### 2. UDP
- **الاستخدام**: نفق UDP بدون اتصال
- **الميزات**: سريع، مناسب للبيانات في الوقت الفعلي
- **المنفذ الافتراضي**: 53

### 3. HTTP
- **الاستخدام**: نفق HTTP مع معالجة الطلبات والاستجابات
- **الميزات**: دعم HTTP headers، معالجة الطلبات
- **المنفذ الافتراضي**: 80

### 4. WebSocket
- **الاستخدام**: نفق WebSocket مع handshake كامل
- **الميزات**: دعم WebSocket frames، handshake تلقائي
- **المنفذ الافتراضي**: 80

## البنية المعمارية

### الملفات الرئيسية

```
include/
├── slipstream_protocols.h          # واجهة النظام الأساسية
src/
├── slipstream_protocols.c          # تنفيذ النظام الأساسي
├── slipstream_udp_tunnel.c         # تنفيذ UDP tunneling
├── slipstream_http_tunnel.c        # تنفيذ HTTP tunneling
└── slipstream_websocket_tunnel.c   # تنفيذ WebSocket tunneling
examples/
└── protocol_example.c              # مثال على الاستخدام
```

### الهياكل الأساسية

#### `slipstream_protocol_type_t`
```c
typedef enum {
    SLIPSTREAM_PROTOCOL_TCP = 0,
    SLIPSTREAM_PROTOCOL_UDP = 1,
    SLIPSTREAM_PROTOCOL_HTTP = 2,
    SLIPSTREAM_PROTOCOL_HTTPS = 3,
    SLIPSTREAM_PROTOCOL_WEBSOCKET = 4,
    SLIPSTREAM_PROTOCOL_ICMP = 5,
    SLIPSTREAM_PROTOCOL_CUSTOM = 99
} slipstream_protocol_type_t;
```

#### `slipstream_protocol_handler_t`
```c
typedef struct {
    int (*init)(void* config);
    void (*cleanup)(void* config);
    int (*create_socket)(void* config, struct sockaddr_storage* target_addr);
    ssize_t (*handle_data)(void* config, int socket_fd, uint8_t* buffer, size_t buffer_size);
    ssize_t (*send_data)(void* config, int socket_fd, const uint8_t* data, size_t data_size);
    bool (*is_ready)(void* config, int socket_fd);
    void* (*get_config)(slipstream_protocol_type_t type);
} slipstream_protocol_handler_t;
```

## كيفية الاستخدام

### 1. استخدام أساسي

```c
#include "slipstream_protocols.h"

// تهيئة مدير البروتوكول
slipstream_protocol_manager_t manager;
slipstream_protocol_manager_init(&manager, SLIPSTREAM_PROTOCOL_UDP);

// إنشاء socket
struct sockaddr_storage target_addr;
// ... إعداد العنوان ...
int socket_fd = manager.handler->create_socket(manager.protocol_config, &target_addr);

// إرسال البيانات
const char* data = "Hello World";
manager.handler->send_data(manager.protocol_config, socket_fd, 
                          (const uint8_t*)data, strlen(data));

// تنظيف
close(socket_fd);
slipstream_protocol_manager_cleanup(&manager);
```

### 2. استخدام UDP Tunnel

```c
#include "slipstream_udp_tunnel.c"

// إنشاء سياق UDP tunnel
slipstream_udp_context_t* udp_ctx;
struct sockaddr_storage target_addr;
// ... إعداد العنوان ...

slipstream_udp_tunnel_create(&udp_ctx, &target_addr, stream_id);

// بدء النفق
int client_socket = accept(listen_socket, NULL, NULL);
slipstream_udp_tunnel_start(udp_ctx, client_socket);

// تنظيف
slipstream_udp_tunnel_destroy(udp_ctx);
```

### 3. استخدام HTTP Tunnel

```c
#include "slipstream_http_tunnel.c"

// إنشاء سياق HTTP tunnel
slipstream_http_context_t* http_ctx;
struct sockaddr_storage target_addr;
// ... إعداد العنوان ...

slipstream_http_tunnel_create(&http_ctx, &target_addr, stream_id, false); // false = HTTP, true = HTTPS

// بدء النفق
int client_socket = accept(listen_socket, NULL, NULL);
slipstream_http_tunnel_start(http_ctx, client_socket);

// تنظيف
slipstream_http_tunnel_destroy(http_ctx);
```

### 4. استخدام WebSocket Tunnel

```c
#include "slipstream_websocket_tunnel.c"

// إنشاء سياق WebSocket tunnel
slipstream_websocket_context_t* ws_ctx;
struct sockaddr_storage target_addr;
// ... إعداد العنوان ...

slipstream_websocket_tunnel_create(&ws_ctx, &target_addr, stream_id, "example.com", 80);

// بدء النفق
int client_socket = accept(listen_socket, NULL, NULL);
slipstream_websocket_tunnel_start(ws_ctx, client_socket);

// تنظيف
slipstream_websocket_tunnel_destroy(ws_ctx);
```

## إضافة بروتوكول جديد

### 1. تعريف البروتوكول

```c
// في slipstream_protocols.h
typedef enum {
    // ... البروتوكولات الموجودة ...
    SLIPSTREAM_PROTOCOL_MY_NEW_PROTOCOL = 6
} slipstream_protocol_type_t;
```

### 2. إنشاء ملف التنفيذ

```c
// في src/slipstream_my_protocol.c
#include "slipstream_protocols.h"

// تعريف handler functions
static int my_protocol_init(void* config) {
    // تهيئة البروتوكول
    return 0;
}

static void my_protocol_cleanup(void* config) {
    // تنظيف البروتوكول
}

static int my_protocol_create_socket(void* config, struct sockaddr_storage* target_addr) {
    // إنشاء socket للبروتوكول
    return socket(target_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
}

// ... باقي الـ handler functions ...

// تعريف الـ handler
static slipstream_protocol_handler_t my_protocol_handler = {
    .init = my_protocol_init,
    .cleanup = my_protocol_cleanup,
    .create_socket = my_protocol_create_socket,
    .handle_data = my_protocol_handle_data,
    .send_data = my_protocol_send_data,
    .is_ready = my_protocol_is_ready,
    .get_config = NULL
};
```

### 3. تحديث النظام الأساسي

```c
// في slipstream_protocols.c
// إضافة البروتوكول الجديد إلى protocol_configs
static const slipstream_protocol_config_t protocol_configs[] = {
    // ... البروتوكولات الموجودة ...
    {
        .type = SLIPSTREAM_PROTOCOL_MY_NEW_PROTOCOL,
        .name = "MY_PROTOCOL",
        .default_port = 8080,
        .requires_connection = true,
        .supports_multiplexing = true,
        .supports_reliability = true
    }
};

// إضافة handler في slipstream_protocol_manager_init
switch (protocol) {
    // ... الحالات الموجودة ...
    case SLIPSTREAM_PROTOCOL_MY_NEW_PROTOCOL:
        manager->handler = &my_protocol_handler;
        break;
}
```

### 4. تحديث CMakeLists.txt

```cmake
# إضافة الملف الجديد إلى COMMON_SOURCES
set(COMMON_SOURCES
    # ... الملفات الموجودة ...
    src/slipstream_my_protocol.c
    # ... باقي الملفات ...
)
```

## التجميع والاختبار

### 1. تجميع المشروع

```bash
mkdir build
cd build
cmake ..
make
```

### 2. تشغيل المثال

```bash
# اختبار TCP
./examples/protocol_example tcp 127.0.0.1 80

# اختبار UDP
./examples/protocol_example udp 127.0.0.1 53

# اختبار HTTP
./examples/protocol_example http 127.0.0.1 80

# اختبار WebSocket
./examples/protocol_example websocket 127.0.0.1 80
```

## المتطلبات

- **OpenSSL**: مطلوب لـ WebSocket tunneling
- **pthread**: مطلوب للـ threading
- **CMake 3.13+**: مطلوب للتجميع

## الميزات المتقدمة

### 1. دعم Multiplexing
البروتوكولات التي تدعم multiplexing يمكنها التعامل مع عدة اتصالات في نفس الوقت.

### 2. إدارة الذاكرة
النظام يدير الذاكرة تلقائياً ويتضمن cleanup functions.

### 3. Thread Safety
جميع العمليات thread-safe باستخدام mutexes.

### 4. Error Handling
نظام شامل لمعالجة الأخطاء مع رسائل واضحة.

## التطويرات المستقبلية

1. **دعم ICMP**: إضافة دعم ICMP tunneling
2. **دعم SCTP**: إضافة دعم SCTP protocol
3. **دعم TLS**: إضافة دعم TLS tunneling
4. **واجهة ويب**: إضافة واجهة ويب لإدارة البروتوكولات
5. **مراقبة الأداء**: إضافة إحصائيات مفصلة للأداء

## المساهمة

للمساهمة في تطوير النظام:

1. Fork المشروع
2. إنشاء branch جديد
3. إضافة البروتوكول الجديد
4. كتابة الاختبارات
5. إرسال Pull Request

## الترخيص

هذا المشروع مرخص تحت نفس ترخيص slipstream الأصلي.
