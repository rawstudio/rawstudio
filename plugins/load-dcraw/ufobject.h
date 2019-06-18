/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufobject.h - UFObject definitions.
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UFOBJECT_H
#define _UFOBJECT_H

/***********************************\
 * UFObject C/C++ common interface *
\***********************************/

/// Type definition for the name of a UFObject.
typedef const char *UFName;

/// UFObject is base class for both the C and C++ interfaces.
typedef struct UFObject UFObject;

/// Events that can be triggered and should be handled by the event handler.
typedef enum {
    uf_value_changed, ///< Value changed.
    uf_default_changed, ///< Default value changed.
    uf_element_added, ///< An UFObject was added to a UFGroup or a UFArray.
    uf_user_data_set, ///< User data was set.
    uf_destroyed ///< UFObject is being destroyed.
} UFEventType;

/// Function prototype for handling events.
typedef void (UFEventHandle)(UFObject *, UFEventType);

/**************************\
 * UFObject C++ interface *
\**************************/

#ifdef __cplusplus

#include <string>
#include <list>
#include <stdexcept> // for std::runtime_error

/**
 * UFObjects are smart data containers, which are suppose to know everything
 * that is needed to handle and manipulated their content.
 *
 * UFObject is an abstract class. There are four UFObject implementations:
 * - UFNumber - holds a number with a defined range and accuracy.
 * - UFNumberArray - holds a fixed length array of numbers.
 * - UFString - holds a string and possibly a list of tokens for this string.
 * - UFGroup - holds a group of UFObjects.
 * - UFIndex - holds an indexed group of UFObjects.
 *
 * There are downcasting definitions from all these implementations down
 * to UFObject. These are needed because each UFObject type has different
 * methods. Downcasting eases the access to these methods. An
 * std::bad_cast exception will be thrown if invalid downcasting is attempted.
 *
 * Each UFObject has a UFName. This name identifies the object and should be
 * unique. It is also used to access UFGroup members.
 *
 * The C++ interface of UFObject throws an exception is case of failure.
 * Therefore, there is no error indication in the return-value of any of
 * the methods. Most exceptions indicated programming errors and can be avoided.
 * Only in the case of UFObject::Set(const char *string), a UFException could
 * result from a user input error.
 *
 * \anchor C-interface
 * The C interface of UFObject is shielded from all exception. Failure will
 * be indicated in the return-value of the calling function and result in a
 * call to UFObject::Message() that sends an error to the console by default.
 * In the case of ufobject_set_string(), UFObject::Message() is not called,
 * since it is assumes that the exception resulted from user input error.
 *
 * \exception UFException is the common exception thrown in case of errors.
 * \exception std::bad_cast is thrown when the downcasting operators fail.
 */
class UFObject
{
public:
    /// Trigger a #uf_destroyed event and destroy the object. An object that
    /// has a Parent() should never be destroyed directly. It will be
    /// destroyed when its parent is destroyed.
    virtual ~UFObject();
    UFName Name() const; ///< Retrieve the name of the UFObject.
    /// Set pointer to general user data.
    /// A #uf_user_data_set event is triggered.
    void SetUserData(void *userData);
    void *UserData(); ///< Retrieve pointer to general user data.
    /// Downcast UFObject to UFNumber.
    operator class UFNumber&();
    /// Downcast const UFObject to const UFNumber.
    operator const class UFNumber&() const;
    /// Downcast UFObject to UFNumberArray.
    operator class UFNumberArray&();
    /// Downcast const UFObject to const UFNumberArray.
    operator const class UFNumberArray&() const;
    /// Downcast UFObject to UFString.
    operator class UFString&();
    /// Downcast const UFObject to const UFString.
    operator const class UFString&() const;
    /// Downcast UFObject to UFGroup.
    operator class UFGroup&();
    /// Downcast const UFObject to const UFGroup.
    operator const class UFGroup&() const;
    /// Downcast UFObject to UFArray.
    operator class UFArray&();
    /// Downcast const UFObject to const UFArray.
    operator const class UFArray&() const;
    bool HasParent() const; ///< Return true if object belongs to a UFGroup.
    /// Return the UFGroup the object belongs too.
    /// A std::logic_error will be thrown if the objects belongs to no group.
    /// \exception UFException is thrown if the object has no parent.
    /// This exception can be avoided with the use of HasParent().
    UFGroup &Parent() const;
    /// Translate object to a string. UFObject takes care of the memory
    /// allocation and freeing of the string.
    virtual const char *StringValue() const;
    /// Create an XML block for the object.
    /// If the object value is its default, create and empty XML block.
    /// \param indent - Controls the XML block indentations.
    virtual std::string XML(const char *indent = "") const;
    /// Send an informational message in case of an error or warning.
    /// This method is used internally in the implementation of UFObject.
    /// Override this method to implement your own message handling.
    /// The default handling is to send the message to the parent object.
    /// If no patent exists, send the message to stderr.
    /// \param format - a printf-like string format.
    virtual void Message(const char *format, ...) const;
    /// Throw a UFException. Use this method to throw exceptions from
    /// within customized Event() methods.
    /// \param format - a printf-like string format.
    void Throw(const char *format, ...) const;
    /// Set the value of the object to the value of the object parameter.
    /// Objects must be of same type and must have the same name. If the
    /// value changes, a #uf_value_changed event is triggered.
    /// \exception UFException is thrown if the two objects do not
    /// have the same Name(). This is probably a programming error.
    virtual void Set(const UFObject &object) = 0;
    /// Set the value of the object from the string value. This is the
    /// reverse of the StringValue() method. If the value changes, an
    /// #uf_value_changed event is triggered.
    /// \exception UFException is thrown if the the string can not be
    /// converted to the object type. This could result from a user input
    /// error.
    virtual void Set(const char *string) = 0;
    /// Return true if object has its default value. For numerical objects,
    /// the values has to the same up to the prescribed accuracy.
    virtual bool IsDefault() const = 0;
    /// Set the current object value to its default value.
    /// A #uf_default_changed event is triggered.
    virtual void SetDefault() = 0;
    /// Reset the object value to its default value. If the value changes,
    /// a #uf_value_changed event is triggered.
    virtual void Reset() = 0;
    /// Set a C-style event handler.
    /// C++ events can be set by overriding the Event() virtual member.
    void SetEventHandle(UFEventHandle *handle);
    /// Handle any #UFEventType event. Override this method to implement your
    /// own event handling. The default handling is to call the event handle
    /// set by SetEventHandle() and in the case of #uf_value_changed, to call
    /// also the parent's Event(). If you override this method, you probably
    /// want to call UFObject::Event() from your own implementation of Event().
    virtual void Event(UFEventType type);
    /// Handle a #uf_value_changed event for the object that originated the
    /// change. This method should be overridden if one wants to change
    /// the values of other objects when the original object value has
    /// changed. It is needed to prevent infinite loops where several
    /// objects keep changing each other. The default method does not
    /// do anything.
    virtual void OriginalValueChangedEvent();
protected:
    /// UFObject 's internal implementation is hidden here.
    class _UFObject *const ufobject;
    /// UFObject is an abstract class, therefore it cannot be constructed
    /// directly.
    explicit UFObject(_UFObject *object);
private:
    UFObject(const UFObject &); // Disable the copy constructor.
    UFObject &operator=(const UFObject &); // Disable the assignment operator.
};

/**
 * UFNumber is a UFObject that holds a number which has an allowed range of
 * values, a specified accuracy and default value.
 */
class UFNumber : public UFObject
{
public:
    /// Construct a UFNumber whose initial value is set to its default value.
    /// The number of accuracy digits effects the format of the StringValue()
    /// of the number. The IsEqual() test is also controlled by
    /// @a accuracyDigits. @a step and @a jump have no direct effect on the
    /// object. They are useful for constructing a GtkAdjustment as in
    /// ufnumber_hscale_new() and ufnumber_spin_button_new().
    ///
    /// @a accuracyDigits, @a step and @a jump are
    /// optional arguments, if they are not given they will be automatically
    /// generated. @a accuracyDigits will be calculated from @a minimum,
    /// @a maximum to given between 3 and 4 significant digits. @a step will
    /// be set to 10 times the accuracy and @a jump to 10 times @a step.
    UFNumber(UFName name, double minimum, double maximum, double defaultValue,
             int accuracyDigits = -1, double step = 0.0, double jump = 0.0);
    const char *StringValue() const;
    /// Return the numerical value of the object. This @a double value can
    /// have better accuracy than @a accuracyDigits. So, for example, after
    /// an <tt>object.Set(1.0/3.0)</tt> command, the result of the condition
    /// <tt>(obj.DoubleValue() == 1.0/3.0)</tt> should be true (but it is never
    /// safe to rely on such behavior for floating-point numbers).
    double DoubleValue() const;
    void Set(const UFObject &object);
    void Set(const char *string);
    /// Set the value of the object to the given number. If the number is
    /// outside of the allowed range, the number will be truncated and the
    /// Message() method will be called to report this incident.
    void Set(double number);
    bool IsDefault() const;
    void SetDefault();
    void Reset();
    /// Return true if object value is equal to @a number up to the prescribed
    /// accuracy.
    bool IsEqual(double number) const;
    double Minimum() const;
    double Maximum() const;
    int AccuracyDigits() const;
    double Step() const;
    double Jump() const;
};

/**
 * UFNumberArray is a UFObject that holds an fixed sized array of numbers.
 */
class UFNumberArray : public UFObject
{
public:
    /// Construct a UFNumberArray with the given @a size. The initial value of
    /// the array elements is set to its default value.
    /// The number of accuracy digits effects the format of the StringValue()
    /// of the number. The IsEqual() test is also controlled by
    /// @a accuracyDigits. @a step and @a jump have no direct effect on the
    /// object. They are useful for constructing a GtkAdjustment as in
    /// ufnumber_array_hscale_new() and ufnumber_array_spin_button_new().
    ///
    /// The object is create with one default value for all elements. Once
    /// SetDefault() is called, each element can have a different default.
    /// \sa @a accuracyDigits, @a step and @a jump are optional arguments,
    /// their default values are discussed in UFNumber::UFNumber().
    UFNumberArray(UFName name, int size, double minimum, double maximum,
                  double defaultValue, int accuracyDigits = 0xff, double step = 0.0,
                  double jump = 0.0);
    const char *StringValue() const;
    /// Return the numerical value of the @a index element of the object.
    /// This @a double value can have better accuracy than @a accuracyDigits.
    /// \sa UFNumber::StringValue() for more information.
    /// \exception UFException is thrown if the index is negative or larger
    /// than (Size()-1). This is probably a programming error.
    double DoubleValue(int index) const;
    void Set(const UFObject &object);
    void Set(const char *string);
    /// Set the value of the @a index element to the given number. If the
    /// number is outside of the allowed range, the number will be truncated
    /// and the Message() method will be called to report this incident.
    /// \exception UFException is thrown if the index is negative or larger
    /// than (Size()-1). This is probably a programming error.
    void Set(int index, double number);
    /// Set the values of all the array elements at once. This is useful if
    /// one wants the #uf_value_changed event to be triggered only once.
    /// @a array[] is assumed to be of the right Size().
    void Set(const double array[]);
    bool IsDefault() const;
    void SetDefault();
    void Reset();
    /// Return true if the @a index element value is equal to @a number
    /// up to the prescribed accuracy.
    /// \exception UFException is thrown if the index is negative or larger
    /// than (Size()-1). This is probably a programming error.
    bool IsEqual(int index, double number) const;
    int Size() const;
    double Minimum() const;
    double Maximum() const;
    int AccuracyDigits() const;
    double Step() const;
    double Jump() const;
};

/**
 * UFString is a UFObject that holds a character string.
 */
class UFString : public UFObject
{
public:
    /// Construct a UFString whose initial value is set to its default value.
    explicit UFString(UFName name, const char *defaultValue = "");
    void Set(const UFObject &object);
    void Set(const char *string);
    bool IsDefault() const;
    void SetDefault();
    /// Set @a string as a default value.
    /// A #uf_default_changed event is triggered.
    void SetDefault(const char *string);
    void Reset();
    /// Return true if object value is equal to @a string.
    bool IsEqual(const char *string) const;
};

/// A list of UFObjects returned by UFGroup or UFArray.
typedef std::list<UFObject *> UFGroupList;

/**
 * UFGroup is a UFObject that contain a group of UFObject elements. This
 * object is considered the Patent() of these elements.
 */
class UFGroup : public UFObject
{
public:
    /// Construct an empty UFGroup, containing no objects.
    /// The @a label is used to index the UFGroup inside a UFArray.
    explicit UFGroup(UFName name, const char *label = "");
    /// Destroy a UFGroup after destroying all the objects it contains.
    ~UFGroup();
    std::string XML(const char *indent = "") const;
    void Set(const UFObject &object);
    void Set(const char *string);
    bool IsDefault() const;
    void SetDefault();
    void Reset();
    /// Return true if the UFGroup contains an object called @a name.
    bool Has(UFName name) const;
    /// Access a UFObject element in a UFGroup.
    /// \exception UFException is thrown if an element with the given name
    /// does not exist. This can be avoided with the use of the Has() method.
    UFObject &operator[](UFName name);
    /// Access a constant UFObject element in a constant UFGroup.
    /// \exception UFException is thrown if an element with the given name
    /// does not exist. This can be avoided with the use of the Has() method.
    const UFObject &operator[](UFName name) const;
    /// Return a list of all UFObjects in the group.
    const UFGroupList List() const;
    /// Add (append) a UFObject to a UFGroup. If the object belonged to
    /// another group before, it will be detached from the original group.
    /// \exception UFException is thrown if UFGroup already contains
    /// an object with the same name. This can be avoided with the use of the
    /// Has() method.
    virtual UFGroup &operator<<(UFObject *object);
    /// Drop an object from the group. The dropped object is returned.
    /// If it is not needed any more it should be deleted to free its memory.
    /// \exception UFException is thrown if an element with the given name
    /// does not exist. This can be avoided with the use of the Has() method.
    /// For UFArray, the index does not get updated.
    UFObject &Drop(UFName name);
    /// Remove all elements from the group.
    /// The removed elements are deleted from memory.
    /// For UFArray, the index does not get updated.
    void Clear();
};

/**
 * UFArray is a UFObject that contain an indexed group of UFObject elements.
 * The array's elements are indexed by their StringValue(). In the current
 * implementation, the StringValue() should not be changed after the
 * UFObject was added to the UFArray.
 */
class UFArray : public UFGroup
{
public:
    /// Construct an empty UFArray, containing no objects.
    explicit UFArray(UFName name, const char *defaultIndex = "");
    std::string XML(const char *indent = "") const;
    void Set(const UFObject &object);
    void Set(const char *string);
    const char *StringValue() const;
    bool IsDefault() const;
    void SetDefault();
    /// Set @a string as a default string value for the UFArray. As opposed
    /// to the SetDefault() method with no arguments, this method does not
    /// changes the default of the array's elements.
    /// A #uf_default_changed event is triggered.
    void SetDefault(const char *string);
    void Reset();
    /// Set the current index position in the array.
    /// Return false if @a index is out of range.
    bool SetIndex(int index);
    /// Retriew the current index location in the array. -1 is returned
    /// if the string index value corresponds to no element's label.
    int Index() const;
    /// Return true if the string index value is equal to @a string.
    bool IsEqual(const char *string) const;
    /// Add (append) a UFObject to a UFArray. If the object belonged to
    /// another array before, it will be detached from the original array.
    /// \exception UFException is thrown if UFArray already contains
    /// an object with the same StringValue. This can be avoided with the
    /// use of the Has() method.
    UFArray &operator<<(UFObject *object);
};

/// This is the common exception thrown by the UFObject implementation.
/// Usually, it represents a programming error. But in the case of
/// UFObject::Set(const char *string), a UFException could result from
/// a user input error.
class UFException : public std::runtime_error
{
public:
    explicit UFException(std::string &Message);
};

#endif // __cplusplus

/*************************\
 * UFObject C interface *
\*************************/

typedef int UFBoolean;
#define UF_FALSE (0)
#define UF_TRUE (!UF_FALSE)

#ifdef __cplusplus
extern "C" {
#endif

/// Delete a UFObject and free its resources. Never use free() on UFObject s.
UFObject *ufobject_delete(UFObject *object);
/// Retrieve the name of the UFObject.
UFName ufobject_name(UFObject *object);
UFObject *ufobject_parent(UFObject *object);
/// Translate object to a string. See UFObject::StringValue() for details.
const char *ufobject_string_value(UFObject *object);
/// Set the value of the object from the string value.
/// Returns false on  failure.
/// See \ref C-interface and UFObject::Set(const char *string) for details.
UFBoolean ufobject_set_string(UFObject *object, const char *string);
/// Copy the value of the source object to the destination object.
/// Returns false on failure.
/// See \ref C-interface and UFObject::Set(const UFObject &object) for details.
UFBoolean ufobject_copy(UFObject *destination, UFObject *source);
/// Create an XML block for the object. The returned buffer should be
/// free()'d by the caller. See UFObject::XML() for details.
char *ufobject_xml(UFObject *object, const char *indent);
void *ufobject_user_data(UFObject *object);
void ufobject_set_user_data(UFObject *object, void *user_data);
void ufobject_set_changed_event_handle(UFObject *object,
                                       UFEventHandle *handle);
/// Return TRUE if object is set to its default value.
UFBoolean ufobject_is_default(UFObject *object);
/// Set the current object value to its default value.
void ufobject_set_default(UFObject *object);
/// Return the numerical value of the object. Returns NaN if object is not a
/// UFNumber. See \ref C-interface and UFNumber::DoubleValue() for more details.
double ufnumber_value(UFObject *object);
/// Set the value of the object to the given number. Returns false if @a object
/// is not a UFNumber. See \ref C-interface and UFNumber::Set(double number)
/// for more details.
UFBoolean ufnumber_set(UFObject *object, double number);
/// Return the numerical value of the @a index element of the object.
/// Returns NaN if @a object is not a UFNumberArray or @a index is out of range.
/// See \ref C-interface and UFNumberArray::DoubleValue() for more details.
double ufnumber_array_value(UFObject *object, int index);
/// Set the value of all the array elements at once.
/// Returns false if @a object is not a UFNumberArray. See \ref C-interface
/// and UFNumberArray::Set(const double array[]) for more details.
UFBoolean ufnumber_array_set(UFObject *object, const double array[]);
/// Return true if string value is equal to @a string.
/// Return false if it is not equal or if object is not a UFString.
/// See \ref C-interface for more details.
UFBoolean ufstring_is_equal(UFObject *object, const char *string);
/// Return true if the UFGroup @a object contains an object called name.
/// Return false if it does not, or if object is not a UFGroup.
/// See \ref C-interface for more details.
UFBoolean ufgroup_has(UFObject *object, UFName name);
/// Return a UFObject element in a UFGroup. Return NULL if element is not found
/// or if @a object is not a UFGroup. See \ref C-interface for more details.
UFObject *ufgroup_element(UFObject *object, UFName name);
/// Add a UFObject to a UFGroup. Return false if UFGroup already contains
/// an object with the same name. See \ref C-interface for more details.
UFBoolean ufgroup_add(UFObject *group, UFObject *object);
/// Drop an object from the group. The dropped object is returned.
/// If it is not needed any more it should be deleted to free its memory.
UFObject *ufgroup_drop(UFObject *group, UFName name);
/// Set the current index position in the array.
UFBoolean ufarray_set_index(UFObject *object, int index);
/// Retriew the current index location in the array. -1 is returned
/// if the string index value corresponds to no element's label.
int ufarray_index(UFObject *object);
/// Return true if array's string value is equal to @a string.
/// Return false if it is not equal or if object is not a UFArray.
/// See \ref C-interface for more details.
UFBoolean ufarray_is_equal(UFObject *object, const char *string);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /*_UFOBJECT_H*/
