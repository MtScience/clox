#ifndef clox_value_h
#define clox_value_h


#include "common.h"


/* Forward declarations */

/**
 * \brief Forward declares the `GC` structure.
 */
typedef struct GC GC;


/**
 * \brief Forward declares the `Obj` structure.
 */
typedef struct Obj Obj;

/**
 * \brief Forward declares the `ObjString` structure.
 */
typedef struct ObjString ObjString;

/**
 * \brief Forward declares the `ObjFunction` structure.
 */
typedef struct ObjFunction ObjFunction;

/**
 * \brief Forward declares the `ObjClosure` structure.
 */
typedef struct ObjClosure ObjClosure;

/**
 * \brief Forward declares the `ObjUpvalue` structure.
 */
typedef struct ObjUpvalue ObjUpvalue;


#ifdef NAN_BOXING


#include <string.h>  /* For memcpy */

/**
 * \brief Lox value type.
 *
 * In case the `NAN_BOXING` macro is set (by the compiler, it is not explicitly written
 * in any file in this implementation), the `Value` is just a single `uint64_t`, different
 * bits of which are used for different purposes.
 */
typedef uint64_t Value;


/* Auxiliary definitions */

/**
 * \brief Auxiliary enumeration for bit tags.
 *
 * There are two unused bits after the exponent, in which information about the value
 * can be kept. This enumeration defines the tags specifying the kind of the value.
 */
enum Tag : uint64_t {
    TAG_NIL   = 1,  /**< Tag for `nil` values. Set to `01`. */
    TAG_FALSE = 2,  /**< Tag for `false` values. Set to `10`. */
    TAG_TRUE  = 3,  /**< Tag for `true` values. Set to `11`. */
};


/**
 * \brief Special constant.
 *
 * This is a value with the bit pattern of `10...0`, \e i.e. all the bits set to zero
 * except for the sign bit.
 */
static constexpr uint64_t signBit  = 0x8000000000000000;

/**
 * \brief Special constant.
 *
 * This is a value with the bit pattern of `0111111111110...0`, \e i.e. it defines a
 * "quiet NaN" pattern.
 */
static constexpr uint64_t QNaN     = 0x7ffc000000000000;

/**
 * \brief Special constant.
 *
 * Represents a Lox `true` value.
 */
static constexpr uint64_t trueVal  = QNaN | TAG_TRUE;

/**
 * \brief Special constant.
 *
 * Represents a Lox `false` value.
 */
static constexpr uint64_t falseVal = QNaN | TAG_FALSE;


/* Constructors */

/*
 * These are documented below, in the "non-NaN boxing part".
 */

static inline Value boolVal(bool value) { return value ? trueVal : falseVal; }
static inline Value nilVal(void)        { return QNaN | TAG_NIL; }
static inline Value objVal(Obj* obj)    { return signBit | QNaN | (uintptr_t)obj; }

static inline Value numberVal(double num) {
    Value value;
    memcpy(&value, &num, sizeof num);
    return value;
}


/* Type-conversions */

static inline bool asBool(Value value) { return value == trueVal; }
static inline Obj* asObj(Value value)  { return (Obj*)(uintptr_t)(value & ~(signBit | QNaN)); }

static inline double asNumber(Value value) {
    double num;
    memcpy(&num, &value, sizeof value);
    return num;
}


/* Type checks */

static inline bool isBool(Value value)   { return value == trueVal || value == falseVal; }
static inline bool isNil(Value value)    { return value == nilVal(); }
static inline bool isNumber(Value value) { return (value & QNaN) != QNaN; }
static inline bool isObj(Value value)    { return (value & (QNaN | signBit)) == (QNaN | signBit); }


#else


/**
 * \brief Lox value types enumeration.
 *
 * An `enum` that lists all the types a Lox value can have.
 */
typedef enum : uint8_t {
    VAL_BOOL,    /**< Specifies a boolean value. */
    VAL_NIL,     /**< Specifies a `nil` value. */
    VAL_NUMBER,  /**< Specifies a numeric value. */
    VAL_OBJ,     /**< Specifies a value that keeps an object reference. */
} ValueType;


/**
 * \struct Value value.h
 * \brief Lox value type.
 *
 * A `struct` that represents a Lox value. Consists only of the value's bit
 * pattern and a type tag, which specifies what that pattern actually holds.
 */
typedef struct {
    union {
        bool   boolean;  /**< Treats a C value as a boolean. */
        double number;   /**< Treats a C value as a `double`. */
        Obj*   obj;      /**< Treats a C value as a reference to a Lox object. */
    } as;            /**< Actual value, kept as a `union` to allow for
                      * bit pattern reinterpretation.
                      */
    ValueType type;  /**< Type tag. */
} Value;


/* Type checks */

/**
 * \defgroup ValueTypeChecks
 * \brief Lox value type checking functions.
 *
 * These functions take a `Value` and test whether it has a certain type.
 *
 * \param[in] value The value to test the type of.
 *
 * \returns Type test result.
 *
 * @{
 */
static inline bool isBool(Value value)   { return value.type == VAL_BOOL;   }
static inline bool isNil(Value value)    { return value.type == VAL_NIL;    }
static inline bool isNumber(Value value) { return value.type == VAL_NUMBER; }
static inline bool isObj(Value value)    { return value.type == VAL_OBJ;    }
/**
 * @}
 */


/* Type conversions */

/**
 * \defgroup ValueTypeConversions
 * \brief Value type conversion functions.
 *
 * These functions take a `Value` and extract data of certain type from its
 * underlying `union`.
 *
 * \param[in] value The value to extract data from.
 *
 * \returns Extracted C value.
 *
 * @{
 */
static inline bool   asBool(Value value)   { return value.as.boolean; }
static inline double asNumber(Value value) { return value.as.number;  }
static inline Obj*   asObj(Value value)    { return value.as.obj;     }
/**
 * @}
 */


/* Constructors */

/**
 * \defgroup ValueConstructors
 * \brief Lox value construsction functions.
 *
 * These functions take C values (booleans, pointers or `double`s) and pack them into
 * `Value` structures.
 *
 * \param[in] value C value that the resulting Lox value should represent.
 *
 * \returns Lox value structure.
 *
 * @{
 */
static inline Value boolVal(bool value)     { return (Value){ .type = VAL_BOOL,   .as = { .boolean = value } }; }
static inline Value nilVal(void)            { return (Value){ .type = VAL_NIL,    .as = { .number = 0 } };      }
static inline Value numberVal(double value) { return (Value){ .type = VAL_NUMBER, .as = { .number = value } };  }
static inline Value objVal(Obj* value)      { return (Value){ .type = VAL_OBJ,    .as = { .obj = value } };     }
/**
 * @}
 */


#endif


/**
 * \struct ValueArray value.h
 * \brief Array of values type.
 *
 * A `struct` that keeps an array of `Value`s and some bookkeeping inforamtion.
 * Standard dynamic array data structure.
 */
typedef struct {
    uint32_t count;     /**< Number of values kept in the structure. */
    uint32_t capacity;  /**< Maximum number of values the structure can keep. */
    Value*   values;    /**< List of `Value`s kept in the `ValueArray`. */
} ValueArray;


/* Interface */

/**
 * \brief Initializes a value array.
 *
 * Takes a newly-created `ValueArray` and fills in its fields, putting the structure
 * in a well-defined state.
 *
 * \param[in,out] array Non-`NULL` pointer to the chunk to be initialized.
 */
void initValueArray(ValueArray array[restrict static 1]);

/**
 * \brief Frees a value array.
 *
 * Takes a `ValueArray` and frees the memory occupied by it.
 *
 * \param[in] gc    Non-`NULL` pointer to a `GC` structure. `gc` must point to the same structure
 *                  that was used for filling the array.
 * \param[in] array Non-`NULL` pointer to the array to be freed.
 */
void freeValueArray(GC* gc, ValueArray array[restrict static 1]);

/**
 * \brief Writes a value to an array.
 *
 * Takes a `ValueArray` and a `Value` and writes the value into the `ValueArray::values` field.
 *
 * \param[in] gc    Non-`NULL` pointer to a `GC` structure. To avoid problems with freeing and
 *                  garbage collection, this should always point to the same structure for a
 *                  given value array.
 * \param[in] array Non-`NULL` pointer to the array to write the value into.
 * \param[in] value The value to be written.
 */
void writeValueArray(GC* gc, ValueArray array[static 1], Value value);


/**
 * \brief Tests values for equality.
 *
 * Checks whether two `Value`s are equal. For booleans and numbers it uses the regular value
 * comparison; for Lox objects it uses pointer equality (basically functioning like Python's
 * `is` operator).
 *
 * This function is marked as `[[reproducible]]`.
 *
 * \param[in] a Left operand.
 * \param[in] b Right operand.
 *
 * \returns Equality test result.
 */
bool valuesEqual(Value a, Value b);

/**
 * \brief Prints a value.
 *
 * Takes a `Value` and prints its numeric value, boolean value, `nil` or object representation,
 * depending on what its `Value::type` is (or its bit pattern, in case `NAN_BOXING` is defined).
 *
 * \param[in] value The value to be printed.
 */
void printValue(Value value);


#endif
