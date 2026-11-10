#ifndef clox_object_h
#define clox_object_h


#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "table.h"
#include "value.h"


/**
 * \defgroup ObjectTypes
 * \brief Object type structures.
 *
 * These structures respresent Lox objects in C.
 *
 * @{
 */

/**
 * \brief Object type enumeration.
 *
 * An `enum` that lists all the supported Lox object types.
 */
typedef enum : uint8_t {
    OBJ_FUNCTION,      /**< Specifies an object as a user-defined function. */
    OBJ_NATIVE,        /**< Specifies an object as a built-in function. */
    OBJ_CLOSURE,       /**< Specifies an object as a closure. */
    OBJ_STRING,        /**< Specifies an object as a string. */
    OBJ_UPVALUE,       /**< Specifies an object as an upvalue. */
    OBJ_CLASS,         /**< Specifies an object as a user-defined class. */
    OBJ_INSTANCE,      /**< Specifies an object as a class instance. */
    OBJ_BOUND_METHOD,  /**< Specifies an object as a bound method. */
} ObjType;


/**
 * \struct Obj object.h
 * \brief Parent type for objects.
 *
 * A `struct` that serves as a common "supertype" for all Lox objects. It must be
 * included in an inheriting structure as its first field to allow for safe casting
 * between pointer types.
 *
 * Implements an intrusive linked list data structure.
 */
struct Obj {
    ObjType     type;      /**< Type tag. Specifies what kind of object it is. */
    bool        isMarked;  /**< Flag that is set during garbage collection if the
                            *   object is reachable.
                            */
    struct Obj* next;      /**< Pointer to the next object in the list. */
};


/**
 * \struct ObjString object.h
 * \brief Lox string type.
 *
 * A `struct` that represents a Lox string.
 */
struct ObjString {
    Obj      obj;      /**< Object header. */
    uint32_t length;   /**< Length of the string in bytes. */
    uint32_t hash;     /**< Hash value for the string. Necessary for hash table
                        *   functionality.
                        */
    char     chars[];  /**< Array of characters constituting the string. */
};


/**
 * \struct ObjUpvalue object.h
 * \brief Lox upvalue type.
 *
 * A `struct` that represents an upvalue. An \e upvalue is a variable defined in an
 * outer scope relative to the function being defined, but used inside it.
 *
 * Implements an intrusive linked list data structure.
 */
struct ObjUpvalue {
    Obj                obj;       /**< Object header. */
    Value*             location;  /**< Points to the value being referred to. */
    Value              closed;    /**< Referenced value. */
    struct ObjUpvalue* next;      /**< Pointer to the next upvalue in the list. */
};


/**
 * \struct ObjFunction object.h
 * \brief Lox function type.
 *
 * A `struct` that represents a Lox function.
 */
struct ObjFunction {
    Obj        obj;           /**< Object header. */
    uint16_t   arity;         /**< Function arity (number of arguments it expects). */
    uint8_t    upvalueCount;  /**< Number of upvalues the function references. */
    Chunk      chunk;         /**< Compiled code of the function. */
    ObjString* name;          /**< Name of the function. */
};


/**
 * \brief Native function signature.
 *
 * Defines a readable type alias for C functions that implement
 * built-in Lox functions.
 */
typedef Value (*NativeFn)(uint8_t argCount, Value args[static argCount]);


/**
 * \struct ObjNative object.h
 * \brief Lox built-in function type.
 *
 * A `struct` that represents a built-in function. In this project, there is only one
 * such function: `clock()`.
 */
typedef struct {
    Obj      obj;       /**< Object header. */
    NativeFn function;  /**< Pointer to the C function that implements the logic. */
} ObjNative;


/**
 * \struct ObjClosure object.h
 * \brief Lox closure type.
 *
 * A `struct` that represents a closure. A \e closure is a function that references some
 * value in the scope where it is defined and keeps this value after the surronding scope
 * is destroyed.
 */
struct ObjClosure {
    Obj          obj;           /**< Object header. */
    ObjFunction* function;      /**< The function associated with the closure. */
    ObjUpvalue** upvalues;      /**< List of pointers to upvalues the `ObjClosure::function`
                                 *   function references.
                                 */
    uint8_t      upvalueCount;  /**< Number of upvalues in the closure. */
};


/**
 * \struct ObjClass object.h
 * \brief Lox class type.
 *
 * A `struct` that represents a Lox class.
 */
typedef struct {
    Obj        obj;      /**< Object header. */
    ObjString* name;     /**< Name of the class. */
    Table      methods;  /**< Hash table that keeps the methods of the class. */
} ObjClass;


/**
 * \struct ObjInstance object.h
 * \brief Lox class instance type.
 *
 * A `struct` that represents a class instance.
 */
typedef struct {
    Obj       obj;     /**< Object header. */
    ObjClass* class;   /**< Pointer to the `ObjClass` object that was used to create
                        *   the instance.
                        */
    Table     fields;  /**< Hash table that keeps the fields of the instance. */
} ObjInstance;


/**
 * \struct ObjBoundMethod object.h
 * \brief Lox bound method type.
 *
 * A `struct` that represents a bound method. A <em>bound method</em> is a method
 * that tracks its receiver object.
 */
typedef struct {
    Obj         obj;       /**< Object header. */
    Value       receiver;  /**< The Lox object, on which the method is called. */
    ObjClosure* method;    /**< Function object that implements the method. */
} ObjBoundMethod;

/**
 * @}
 */


/* Type checks */

/**
 * \brief Checks an object's type.
 *
 * Takes a `Value` and an `ObjType` and determines whether the object held in the
 * value has the specified type.
 *
 * \param[in] value The value to check the type of.
 * \param[in] type  The type of object to check against.
 *
 * \returns `true` if the value has the specified type, `false` if the value is not
 *          a Lox object or the type is different.
 */
static inline bool isObjType(Value value, ObjType type) {
    return isObj(value) && asObj(value)->type == type;
}


/**
 * \brief Extracts an object's type.
 *
 * Takes a `Value` and returns the type of object it represents. The function assumes
 * that the value is a valid Lox object.
 *
 * \param[in] value The value, whose object type is to be extracted.
 *
 * \returns The object type of the input value.
 */
static inline ObjType objType(Value value) {
    return asObj(value)->type;
}

/**
 * \defgroup ObjectTypeChecks
 * \brief Lox object type checking functions.
 *
 * These functions take a `Value` and, assuming it represents a Lox object,
 * test whether the object has a certain type.
 *
 * \param[in] value The value to test the type of.
 *
 * \returns Type test result.
 *
 * @{
 */
static inline bool isString(Value value)      { return isObjType(value, OBJ_STRING);       }
static inline bool isFunction(Value value)    { return isObjType(value, OBJ_FUNCTION);     }
static inline bool isNative(Value value)      { return isObjType(value, OBJ_NATIVE);       }
static inline bool isClosure(Value value)     { return isObjType(value, OBJ_CLOSURE);      }
static inline bool isClass(Value value)       { return isObjType(value, OBJ_CLASS);        }
static inline bool isInstance(Value value)    { return isObjType(value, OBJ_INSTANCE);     }
static inline bool isBoundMethod(Value value) { return isObjType(value, OBJ_BOUND_METHOD); }
/**
 * @}
 */


/* Type conversions */

/**
 * \defgroup ObjectTypeConversions
 * \brief Object type conversion functions.
 *
 * These functions take a `Value` and, assuming it represents a Lox object,
 * extract the pointer to data from it and cast that pointer to specific type.
 *
 * \param[in] value The value to perform type conversion on.
 *
 * \returns The pointer to a Lox object type.
 *
 * @{
 */

/**
 * \returns The C string referenced by the value.
 */
static inline char*           asCString(Value value)     { return ((ObjString*)asObj(value))->chars;    }
static inline ObjString*      asString(Value value)      { return (ObjString*)asObj(value);             }
static inline ObjFunction*    asFunction(Value value)    { return (ObjFunction*)asObj(value);           }

/**
 * \returns The pointer to the C function that implements
 *          the behaviour.
 */
static inline NativeFn        asNative(Value value)      { return ((ObjNative*)asObj(value))->function; }
static inline ObjClosure*     asClosure(Value value)     { return (ObjClosure*)asObj(value);            }
static inline ObjClass*       asClass(Value value)       { return (ObjClass*)asObj(value);              }
static inline ObjInstance*    asInstance(Value value)    { return (ObjInstance*)asObj(value);           }
static inline ObjBoundMethod* asBoundMethod(Value value) { return (ObjBoundMethod*)asObj(value);        }
/**
 * @}
 */


/* Constructors */

/**
 * \brief Constructs a Lox string.
 *
 * Takes a `VM` and an array of `char`s and creates a new `ObjString`. This function assumes
 * the caller does not own the source array, so it leaves it unchanged.
 *
 * \param[in] vm     Non-`NULL` pointer to the virtual machine that will manage the resulting
 *                   Lox string.
 * \param[in] length The length of the string being processed
 * \param[in] chars  An array of characters that contains the string data. Does not have to be
 *                   a null-terminated C string.
 *
 * \returns The pointer to the newly-created or previously interned (if available) Lox string.
 */
ObjString* copyString(VM* vm, uint32_t length, char const chars[static length]);

/**
 * \brief Constructs a Lox string.
 *
 * Takes a `VM` and an array of `char`s and creates a new `ObjString`. Unlike `copyString()`,
 * it assumes the caller \e does own the source array, so it frees it if an interned string is
 * found.
 *
 * \param[in] vm     Non-`NULL` pointer to the virtual machine that will manage the resulting
 *                   Lox string.
 * \param[in] length The length of the string being processed
 * \param[in] chars  An array of characters that contains the string data. Does not have to be
 *                   a null-terminated C string.
 *
 * \returns The pointer to the newly-created or previously interned (if available) Lox string.
 */
ObjString* takeString(VM* vm, uint32_t length, char chars[static length]);

/**
 * \defgroup ObjectConstructors
 * \brief Lox object construction functions.
 *
 * These functions take a `GC` and necessary data and construct a new heap-allocated object value.
 *
 * String construction is somewhat special and is handled by `copyString()` and `takeString()`.
 *
 * \param[in] gc Non-`NULL` pointer to a garbage collector that will manage the object.
 *
 * \returns The pointer to a newly-created structure of appropriate type.
 *
 * @{
 */

/**
 * \param[in] slot Non-`NULL` pointer to the value to make an upvalue from.
 */
ObjUpvalue* newUpvalue(GC gc[static 1], Value slot[static 1]);

ObjFunction* newFunction(GC gc[static 1]);

/**
 * \param[in] function Pointer to the C function that implements the built-in's behaviour.
 */
ObjNative* newNative(GC gc[static 1], NativeFn function);

/**
 * \param[in] function Non-`NULL` pointer to the Lox function to make a closure from.
 */
ObjClosure* newClosure(GC gc[static 1], ObjFunction function[static 1]);

/**
 * \param[in] name Non-`NULL` pointer to a string object containing the name on the class.
 */
ObjClass* newClass(GC gc[static 1], ObjString* name);

/**
 * \param[in] class Non-`NULL` pointer to the class whose instance is being created.
 */
ObjInstance* newInstance(GC gc[static 1], ObjClass class[static 1]);

/**
 * \param[in] receiver The value that keeps an instance, on which the method will be called.
 * \param[in] method   Non-`NULL` pointer to a closure representing the method.
 */
ObjBoundMethod* newBoundMethod(GC gc[static 1], Value receiver, ObjClosure method[static 1]);
/**
 * @}
 */


/**
 * \brief Prints a Lox object.
 *
 * Takes a `Value` and prints the Lox object held within according to its type.
 *
 * \param[in] value The value to be printed. Expected to be a Lox object, not just
 *                  any valid `Value`.
 */
void printObject(Value value);


#endif
