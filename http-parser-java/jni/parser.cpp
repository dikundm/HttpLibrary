#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string.h>
#include "com_adguard_http_parser_NativeParser.h"
#include "com_adguard_http_parser_HttpMessage.h"
#include "com_adguard_http_parser_HttpMessage_HttpHeaderField.h"
#include "../../http-parser/src/parser.h"
#include "callbacks.h"

static void processError(JNIEnv *env, int returnCode, connection_context *context);
static void processError(JNIEnv *env, int returnCode, const char *message);

/**
 * Invoke parser_connect(), process errors and return the native pointer to connection context
 * @param env JNI env
 * @param cls NativeParser class
 * @param parserNativePtr Pointer to parser context (from NativeParser object)
 * @param id Connection id
 * @param callbacks NativeParser$Callbacks object
 * @return Pointer to connection context
 */
jlong Java_com_adguard_http_parser_NativeParser_connect(JNIEnv *env, jclass cls, jlong parserNativePtr, jlong id,
                                                        jobject callbacks) {
    connection_info *info = new connection_info;
    snprintf(info->endpoint_1, ENDPOINT_MAXLEN, "%s", "something1");
    snprintf(info->endpoint_2, ENDPOINT_MAXLEN, "%s", "something2");

    connection_context *connection_ctx;
    parser_context *parser_ctx = (parser_context *) parserNativePtr;
    int r = parser_connect(parser_ctx, id, &parserCallbacks, &connection_ctx);
    if (r != 0) {
        char message[256];
        snprintf(message, 256, "parser_connect() returned %d", r);
        processError(env, r, message);
    } else {
        new Callbacks(env, callbacks, connection_ctx);
    }

    return (jlong) connection_ctx;
}

/**
 * Tells parser that one of sides of connection was disconnected
 * @param env JNI env
 * @param cls NativeParser class
 * @param connectionPtr Pointer to connection context (from NativeConnection object)
 * @param direction Transfer direction
 */
void Java_com_adguard_http_parser_NativeParser_disconnect0(JNIEnv *env, jclass cls, jlong connectionPtr,
                                                          jint direction) {
    connection_context *context = (connection_context *) connectionPtr;
    int r = parser_disconnect(context, (transfer_direction_t) direction);
    processError(env, r, context);
}

/**
 * Processes input from local/remote side
 * @param env JNI env
 * @param cls NativeParser class
 * @param connectionPtr Pointer to connection context (from NativeConnection object)
 * @param direction Transfer direction
 * @param bytes Input data
 */
void Java_com_adguard_http_parser_NativeParser_input0(JNIEnv *env, jclass cls, jlong connectionPtr, jint direction,
                                                     jbyteArray bytes) {
    connection_context *context = (connection_context *) connectionPtr;
    jbyte *data = env->GetByteArrayElements(bytes, NULL);
    int len = env->GetArrayLength(bytes);
    int r = parser_input(context, (transfer_direction_t) direction, (const char *) data, len);
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    processError(env, r, context);
}

/**
 * Closes connection, tell parser to free corresponding data structures
 * @param env JNI env
 * @param cls NativeParser class
 * @param connectionPtr Pointer to connection context (from NativeConnection object)
 */
void Java_com_adguard_http_parser_NativeParser_closeConnection(JNIEnv *env, jclass cls, jlong connectionPtr) {
    connection_context *context = (connection_context *) connectionPtr;
    int r = parser_connection_close(context);
    // `context' memory is freed at this point
    processError(env, r, "");

    // Delete callbacks
    delete Callbacks::get(context);
}

/**
 * Get connection id by N
 * @param env JNI env
 * @param cls NativeParser class
 * @param connectionPtr Pointer to connection context (from NativeConnection object)
 * @return Connection id
 */
jlong Java_com_adguard_http_parser_NativeParser_getConnectionId(JNIEnv *env, jclass cls, jlong connectionPtr) {
    connection_context *context = (connection_context *) connectionPtr;
    return (jlong) connection_get_id(context);
}

/**
 * NativeParser constructor.
 * Allocate memory and store pointer in pointer field
 * @param env JNI env
 * @param cls NativeParser class
 * @param parser NativeParser object
 * @param loggerPtr Pointer to logger context (from NativeLogger object)
 */
void Java_com_adguard_http_parser_NativeParser_init(JNIEnv *env, jclass cls, jobject parser, jlong loggerPtr) {
    fprintf(stderr, "NativeParser.init()\n");

    jfieldID parserCtxPtr = env->GetFieldID(cls, "parserCtxPtr", "J");
    if (parserCtxPtr == NULL) {
        return;
    }

    logger *log = (logger *) loggerPtr;
    parser_context *parser_ctx;
    int r = parser_create(log, &parser_ctx);
    if (r == 0) {
        env->SetLongField(parser, parserCtxPtr, (jlong) parser_ctx);
    } else {
        env->ThrowNew(env->FindClass("java/io/Exception"), "NativeParser()");
    }
}

/**
 * Closes all parser connections. Additional work is done on Java side (closing every connection)
 * @param env JNI env
 * @param cls NativeParser class
 * @param parserPtr Pointer to parser context (from NativeParser object)
 */
void Java_com_adguard_http_parser_NativeParser_closeParser(JNIEnv *env, jclass cls, jlong parserPtr) {
    parser_context *parser_ctx = (parser_context *) parserPtr;
    int r = parser_destroy(parser_ctx);
    if (r != 0) {
        env->ThrowNew(env->FindClass("java/io/Exception"), "NativeParser.close()");
    }
}

// Utility methods

/**
 * Get URL from HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return URL
 */
jstring Java_com_adguard_http_parser_HttpMessage_getUrl(JNIEnv * env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    return env->NewStringUTF(message->url);
}

/**
 * Set URL in HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param value URL
 */
void Java_com_adguard_http_parser_HttpMessage_setUrl(JNIEnv *env, jclass cls, jlong nativePtr, jstring value) {
    http_message *message = (http_message *) nativePtr;
    jboolean isCopy;
    const char *chars = env->GetStringUTFChars(value, &isCopy);
    size_t len = strlen(chars);
    http_message_set_url(message, chars, len);
    if (isCopy) {
        env->ReleaseStringUTFChars(value, chars);
    }
}

/**
 * Get HTTP status text from HttpMessage (if status text is set, HttpMessage is HTTP response, otherwise it is request)
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return HTTP status text
 */
jstring Java_com_adguard_http_parser_HttpMessage_getStatus(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    return env->NewStringUTF(message->status);
}

/**
 *  Get HTTP status text from HttpMessage (if status text is set, HttpMessage is HTTP response, otherwise it is request)
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param status HTTP status text
 */
void Java_com_adguard_http_parser_HttpMessage_setStatus(JNIEnv *env, jclass cls, jlong nativePtr, jstring status) {
    http_message *message = (http_message *) nativePtr;
    jboolean isCopy;
    const char *statusText = env->GetStringUTFChars(status, &isCopy);
    http_message_set_status(message, statusText, strlen(statusText));
    if (isCopy) {
        env->ReleaseStringUTFChars(status, statusText);
    }
}

/**
 * Get HTTP status code from HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return HTTP status code
 */
jint Java_com_adguard_http_parser_HttpMessage_getStatusCode(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    return message->status_code;
}

/**
 * Set HTTP status code in HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param code HTTP status code
 */
void Java_com_adguard_http_parser_HttpMessage_setStatusCode(JNIEnv *env, jclass cls, jlong nativePtr, jint code) {
    http_message *message = (http_message *) nativePtr;
    http_message_set_status_code(message, code);
}

/**
 * Get HTTP method name from HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return HTTP method name
 */
jstring Java_com_adguard_http_parser_HttpMessage_getMethod(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    return env->NewStringUTF(message->method);
}

/**
 * Get HTTP headers from HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return List of pointers to http_header_field structure (wrapped into HttpHeaderField on Java side)
 */
jlongArray Java_com_adguard_http_parser_HttpMessage_getHeaders(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    jlong nativePtrs[message->field_count];
    http_header_field *parameter = message->fields;
    for (size_t i = 0; i < message->field_count; i++) {
        nativePtrs[i] = (jlong) parameter;
        ++parameter;
    }
    jlongArray array = env->NewLongArray(message->field_count);
    env->SetLongArrayRegion(array, 0, message->field_count, nativePtrs);
    return array;
}

/**
 * Add new HTTP header to HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param fieldName Field name
 * @param value Field value
 */
void Java_com_adguard_http_parser_HttpMessage_addHeader(JNIEnv *env, jclass cls, jlong nativePtr, jstring fieldName, jstring value) {
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    jboolean valueCharsIsCopy;
    const char *valueChars = env->GetStringUTFChars(value, &valueCharsIsCopy);
    http_message *message = (http_message *) nativePtr;
    http_message_add_header_field(message, fieldChars, strlen(fieldChars));
    http_message_set_header_field(message, fieldChars, strlen(fieldChars), valueChars, strlen(valueChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
    if (valueCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, valueChars);
    }
}

/**
 * Remove HTTP header field with given name
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param fieldName Name to field to delete
 */
void Java_com_adguard_http_parser_HttpMessage_removeHeader(JNIEnv *env, jclass cls, jlong nativePtr, jstring fieldName) {
    http_message *message = (http_message *) nativePtr;
    jboolean fieldCharsIsCopy;
    const char *fieldChars = env->GetStringUTFChars(fieldName, &fieldCharsIsCopy);
    http_message_del_header_field(message, fieldChars, strlen(fieldChars));
    if (fieldCharsIsCopy) {
        env->ReleaseStringUTFChars(fieldName, fieldChars);
    }
}

/**
 * Get field name from HttpHeaderField
 * @param env JNI env
 * @param cls HttpHeaderField class
 * @param nativePtr Pointer to http_header_field structure (from HttpMessage$HttpheaderField)
 * @return Field name
 */
jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getKey(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_header_field *parameter = (http_header_field *) nativePtr;
    return env->NewStringUTF(parameter->name);
}

/**
 * Get field value from HttpHeaderField
 * @param env JNI env
 * @param cls HttpHeaderField class
 * @param nativePtr Pointer to http_header_field structure (from HttpMessage$HttpheaderField)
 * @return Field value
 */
jstring Java_com_adguard_http_parser_HttpMessage_00024HttpHeaderField_getValue(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_header_field *parameter = (http_header_field *) nativePtr;
    return env->NewStringUTF(parameter->value);
}

/**
 * Get HttpMessage serialized length
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return Length of serialized HttpMessage
 */
jint Java_com_adguard_http_parser_HttpMessage_sizeBytes(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(message, &length);
    free(message_raw);
    return (jint) length;
}

/**
 * Serialize HttpMessage into correct HTTP request/response
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return New byte array containing correct HTTP/1.1 request/response
 */
jbyteArray Java_com_adguard_http_parser_HttpMessage_getBytes__J(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(message, &length);

    jbyteArray arr = env->NewByteArray((jsize) length);
    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
    return arr;
}

/**
 * Serialize HttpMessage into correct HTTP request/response and write it to the given byte array
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @param arr Byte array for response to write
 */
void Java_com_adguard_http_parser_HttpMessage_getBytes__J_3B(JNIEnv *env, jclass cls, jlong nativePtr, jbyteArray arr) {
    http_message *message = (http_message *) nativePtr;
    size_t length = 0;
    char *message_raw = http_message_raw(message, &length);

    env->SetByteArrayRegion(arr, 0, (jsize) length, (jbyte *) message_raw);
    free(message_raw);
}

/**
 * Clone HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to http_message structure (from HttpMessage)
 * @return Pointer to new http_message structure (wrapped into new HttpMessage on Java side)
 */
jlong Java_com_adguard_http_parser_HttpMessage_clone(JNIEnv *env, jclass cls, jlong nativePtr) {
    if (nativePtr == 0)
        return 0;
    return (jlong) http_message_clone((http_message *) nativePtr);
}

/**
 * Create empty HttpMessage
 * @param env JNI env
 * @param cls HttpMessage class
 * @return Pointer to new http_message structure (wrapped into new HttpMessage on Java side)
 */
jlong Java_com_adguard_http_parser_HttpMessage_createHttpMessage(JNIEnv *env, jclass cls) {
    http_message *message = (http_message *) malloc(sizeof(http_message));
    memset(message, 0, sizeof(http_message));
    return (jlong) message;
}

/**
 * Free HttpMessage structure
 * @param env JNI env
 * @param cls HttpMessage class
 * @param nativePtr Pointer to new http_message structure (from HttpMessage)
 */
void Java_com_adguard_http_parser_HttpMessage_free(JNIEnv *env, jclass cls, jlong nativePtr) {
    http_message *message = (http_message *) nativePtr;
    http_message_free(message);
}

/**
 * Throws exception corresponding to error code
 * @param env JNI env
 * @param returnCode Error code
 * @param context Connection context
 */
static void processError(JNIEnv *env, int returnCode, connection_context *context) {
    processError(env, returnCode, connection_get_error_message(context));
}

/**
 * Throws exception corresponding to error code
 * @param env JNI env
 * @param returnCode Error code
 * @param message Error message
 */
static void processError(JNIEnv *env, int returnCode, const char *message) {
    if (returnCode == PARSER_OK) {
        return;
    }

    char final_message[256];
    switch ((error_type_t) returnCode) {
        case PARSER_NULL_POINTER_ERROR:
            env->ThrowNew(env->FindClass("java/lang/NullPointerException"), message);
            break;
        case PARSER_HTTP_PARSE_ERROR:
            snprintf(final_message, 256, "HTTP parse error: %s", message);
            env->ThrowNew(env->FindClass("java/io/IOException"), final_message);
            break;
        case PARSER_ZLIB_ERROR:
            snprintf(final_message, 256, "Zlib error: %s", message);
            env->ThrowNew(env->FindClass("java/io/IOException"), final_message);
            break;
        case PARSER_INVALID_ARGUMENT_ERROR:
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), message);
            break;
        case PARSER_ALREADY_CONNECTED_ERROR:
            env->ThrowNew(env->FindClass("java/io/IOException"), message);
            break;
        default:
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), message);
            break;
    }
}
