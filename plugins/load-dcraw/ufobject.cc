/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufobject.cc - UFObject C++ implementation and C interface.
 * Copyright 2004-2016 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ufobject.h"
#define G_LOG_DOMAIN "UFObject"
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h> // for strcmp
#include <stdio.h> // for sscanf
#include <math.h> // for pow, log, floor
#include <algorithm> // for std::max
#include <map> // for std::map
#include <stdexcept> // for std::logic_error
#include <typeinfo> // for std::bad_cast
#include <limits> // for std::numeric_limits<double>::quiet_NaN()

/***************************\
 * UFObject implementation *
\***************************/

class _UFObject
{
public:
    const UFName Name;
    void *UserData;
    char *String;
    class _UFGroup *Parent;
    UFEventHandle *EventHandle;
    explicit _UFObject(UFName name) : Name(name), UserData(NULL), String(NULL),
        Parent(NULL), EventHandle(NULL) { }
    virtual ~_UFObject() {
        g_free(String);
        if (Parent != NULL)
            g_warning("%s: Destroyed while having a parent.", Name);
    }
    virtual bool Changing() const;
    virtual void SetChanging(bool state);
    void CallValueChangedEvent(UFObject *that) {
        bool saveChanging = Changing();
        if (!Changing()) {
            SetChanging(true);
            that->OriginalValueChangedEvent();
        }
        that->Event(uf_value_changed);
        SetChanging(saveChanging);
    }
};

UFObject::UFObject(_UFObject *object) : ufobject(object) { }

UFObject::~UFObject()
{
    Event(uf_destroyed);
    delete ufobject;
}

UFName UFObject::Name() const
{
    return ufobject->Name;
}

void UFObject::SetUserData(void *userData)
{
    ufobject->UserData = userData;
    Event(uf_user_data_set);
}

void *UFObject::UserData()
{
    return ufobject->UserData;
}

UFObject::operator class UFNumber&()
{
    return dynamic_cast<UFNumber &>(*this);
}

UFObject::operator const class UFNumber&() const
{
    return dynamic_cast<const UFNumber &>(*this);
}

UFObject::operator class UFNumberArray&()
{
    return dynamic_cast<UFNumberArray &>(*this);
}

UFObject::operator const class UFNumberArray&() const
{
    return dynamic_cast<const UFNumberArray &>(*this);
}

UFObject::operator class UFString&()
{
    return dynamic_cast<UFString &>(*this);
}

UFObject::operator const class UFString&() const
{
    return dynamic_cast<const UFString &>(*this);
}

UFObject::operator class UFGroup&()
{
    return dynamic_cast<UFGroup &>(*this);
}

UFObject::operator const class UFGroup&() const
{
    return dynamic_cast<const UFGroup &>(*this);
}

UFObject::operator class UFArray&()
{
    return dynamic_cast<UFArray &>(*this);
}

UFObject::operator const class UFArray&() const
{
    return dynamic_cast<const UFArray &>(*this);
}

bool UFObject::HasParent() const
{
    return ufobject->Parent != NULL;
}

const char *UFObject::StringValue() const
{
    return ufobject->String;
}

std::string UFObject::XML(const char *indent) const
{
    if (IsDefault())
        return "";
    char *value = g_markup_escape_text(StringValue(), -1);
    std::string str = (std::string)indent +
                      "<" + Name() + ">" + value + "</" + Name() + ">\n";
    g_free(value);
    return str;
}

void UFObject::Message(const char *format, ...) const
{
    if (format == NULL)
        return;
    va_list ap;
    va_start(ap, format);
    char *message = g_strdup_vprintf(format, ap);
    va_end(ap);
    if (HasParent()) {
        Parent().Message("%s: %s", Name(), message);
    } else {
        fprintf(stderr, "%s: %s\n", Name(), message);
    }
    g_free(message);
}

void UFObject::Throw(const char *format, ...) const
{
    if (format == NULL)
        return;
    va_list ap;
    va_start(ap, format);
    char *message = g_strdup_vprintf(format, ap);
    va_end(ap);
    std::string mess(message);
    g_free(message);
    throw UFException(mess);
}

void UFObject::SetEventHandle(UFEventHandle *handle)
{
    ufobject->EventHandle = handle;
}

void UFObject::Event(UFEventType type)
{
    if (ufobject->EventHandle != NULL)
        (*ufobject->EventHandle)(this, type);
    if (type == uf_value_changed && HasParent())
        Parent().Event(type);
}

void UFObject::OriginalValueChangedEvent() { }

/***************************\
 * UFNumber implementation *
\***************************/

class _UFNumberCommon : public _UFObject
{
public:
    double Minimum;
    double Maximum;
    const int AccuracyDigits;
    const double Accuracy;
    const double Step;
    const double Jump;
    _UFNumberCommon(UFName name, double minimum, double maximum,
                    int accuracyDigits, double step, double jump) :
        _UFObject(name), Minimum(minimum), Maximum(maximum),
        AccuracyDigits(std::max(accuracyDigits < 0 ?
                                3 - (int)floor(log(Maximum - Minimum) / log(10.0)) :
                                accuracyDigits, 0)),
        Accuracy(pow(10.0, -AccuracyDigits)),
        Step(step == 0.0 ? Accuracy * 10.0 : step),
        Jump(jump == 0.0 ? Step * 10.0 : jump) { }
};

class _UFNumber : public _UFNumberCommon
{
public:
    double Number;
    double Default;
    _UFNumber(UFName name, double defaultValue, double minimum, double maximum,
              int accuracyDigits, double step, double jump) :
        _UFNumberCommon(name, minimum, maximum, accuracyDigits, step, jump),
        Number(defaultValue), Default(defaultValue) { }
};

#define ufnumber (static_cast<_UFNumber *>(ufobject))

UFNumber::UFNumber(UFName name, double minimum, double maximum,
                   double defaultValue, int accuracyDigits, double step, double jump) :
    UFObject(new _UFNumber(name, defaultValue, minimum, maximum,
                           accuracyDigits, step, jump)) { }

const char *UFNumber::StringValue() const
{
    g_free(ufnumber->String);
    ufnumber->String = g_strdup_printf("%.*f", ufnumber->AccuracyDigits,
                                       ufnumber->Number);
    return ufnumber->String;
}

double UFNumber::DoubleValue() const
{
    return ufnumber->Number;
}

void UFNumber::Set(const UFObject &object)
{
    if (this == &object) // Avoid self-assignment
        return;
    // We are comparing the strings ADDRESSes not values.
    if (Name() != object.Name())
        Throw("Object name mismatch with '%s'", object.Name());
    const UFNumber &number = object;
    Set(number.DoubleValue());
}

void UFNumber::Set(const char *string)
{
    double number;
    int count = sscanf(string, "%lf", &number);
    if (count != 1)
        Throw("String '%s' is not a number", string);
    Set(number);
}

void UFNumber::Set(double number)
{
    if (number > Maximum()) {
        Message(_("Value %.*f too large, truncated to %.*f."),
                AccuracyDigits(), number, AccuracyDigits(), Maximum());
        number = Maximum();
    } else if (number < Minimum()) {
        Message(_("Value %.*f too small, truncated to %.*f."),
                AccuracyDigits(), number, AccuracyDigits(), Minimum());
        number = Minimum();
    }
    if (!this->IsEqual(number)) {
        ufnumber->Number = number;
        ufnumber->CallValueChangedEvent(this);
    }
    // When numbers are equal up to Accuracy, we still want the new value
    ufnumber->Number = number;
}

bool UFNumber::IsDefault() const
{
    return this->IsEqual(ufnumber->Default);
}

void UFNumber::SetDefault()
{
    ufnumber->Default = ufnumber->Number;
    Event(uf_default_changed);
}

void UFNumber::Reset()
{
    Set(ufnumber->Default);
}

bool UFNumber::IsEqual(double number) const
{
    int oldValue = floor(ufnumber->Number / ufnumber->Accuracy + 0.5);
    int newValue = floor(number / ufnumber->Accuracy + 0.5);
    return oldValue == newValue;
}

double UFNumber::Minimum() const
{
    return ufnumber->Minimum;
}

double UFNumber::Maximum() const
{
    return ufnumber->Maximum;
}

int UFNumber::AccuracyDigits() const
{
    return ufnumber->AccuracyDigits;
}
double UFNumber::Step() const
{
    return ufnumber->Step;
}

double UFNumber::Jump() const
{
    return ufnumber->Jump;
}

/********************************\
 * UFNumberArray implementation *
\********************************/

class _UFNumberArray : public _UFNumberCommon
{
public:
    const int Size;
    double *const Array;
    double *const Default;
    _UFNumberArray(UFName name, int size,
                   double minimum, double maximum, double defaultValue,
                   int accuracyDigits, double step, double jump) :
        _UFNumberCommon(name, minimum, maximum, accuracyDigits, step, jump),
        Size(size), Array(new double[size]), Default(new double[size]) {
        for (int i = 0; i < size; i++)
            Array[i] = defaultValue;
        for (int i = 0; i < size; i++)
            Default[i] = defaultValue;
    }
    ~_UFNumberArray() {
        delete [] Array;
        delete [] Default;
    }
    bool SilentChange(UFNumberArray *that, int index, double number) {
        if (index < 0 || index >= Size)
            that->Throw("index (%d) out of range 0..%d", index, Size - 1);
        if (number > Maximum) {
            that->Message(_("Value %.*f too large, truncated to %.*f."),
                          AccuracyDigits, number, AccuracyDigits, Maximum);
            number = Maximum;
        } else if (number < Minimum) {
            that->Message(_("Value %.*f too small, truncated to %.*f."),
                          AccuracyDigits, number, AccuracyDigits, Minimum);
            number = Minimum;
        }
        if (!that->IsEqual(index, number)) {
            Array[index] = number;
            return true;
        }
        // When numbers are equal up to Accuracy, we still want the new value
        Array[index] = number;
        return false;
    }
};

#define ufnumberarray (static_cast<_UFNumberArray *>(ufobject))

#define _uf_max_string 80

UFNumberArray::UFNumberArray(UFName name, int size, double minimum,
                             double maximum, double defaultValue,
                             int accuracyDigits, double step, double jump) :
    UFObject(new _UFNumberArray(name, size, minimum, maximum, defaultValue,
                                accuracyDigits, step, jump)) { }

const char *UFNumberArray::StringValue() const
{
    g_free(ufnumberarray->String);
    std::string str = "";
    char num[_uf_max_string];
    for (int i = 0; i < Size(); i++) {
        g_snprintf(num, _uf_max_string, "%.*f",
                   ufnumberarray->AccuracyDigits, ufnumberarray->Array[i]);
        str += num;
        if (i < Size() - 1)
            str += " ";
    }
    ufnumberarray->String = g_strdup(str.c_str());
    return ufnumberarray->String;
}

double UFNumberArray::DoubleValue(int index) const
{
    if (index < 0 || index >= Size())
        Throw("index (%d) out of range 0..%d", index, Size() - 1);
    return ufnumberarray->Array[index];
}

void UFNumberArray::Set(const UFObject &object)
{
    if (this == &object) // Avoid self-assignment
        return;
    // We are comparing the strings ADDRESSes not values.
    if (Name() != object.Name())
        Throw("Object name mismatch with '%s'", object.Name());
    const UFNumberArray &array = object;
    if (Size() != array.Size())
        Throw("Object size mismatch %d != %d", Size(), array.Size());
    bool changed = false;
    for (int i = 0; i < Size(); i++)
        changed |= ufnumberarray->SilentChange(this, i, array.DoubleValue(i));
    if (changed)
        ufnumberarray->CallValueChangedEvent(this);
}

void UFNumberArray::Set(const char *string)
{
    char **token = g_strsplit(string, " ", Size());
    for (int i = 0; i < Size(); i++) {
        if (token[i] == NULL) {
            Set(i, ufnumberarray->Default[i]);
        } else {
            double number;
            int count = sscanf(token[i], "%lf", &number);
            if (count != 1)
                Throw("String '%s' is not a number", string);
            Set(i, number);
        }
    }
    g_strfreev(token);
}

void UFNumberArray::Set(int index, double number)
{
    bool changed = ufnumberarray->SilentChange(this, index, number);
    if (changed)
        ufnumberarray->CallValueChangedEvent(this);
}

void UFNumberArray::Set(const double array[])
{
    bool changed = false;
    for (int i = 0; i < Size(); i++)
        changed |= ufnumberarray->SilentChange(this, i, array[i]);
    if (changed)
        ufnumberarray->CallValueChangedEvent(this);
}

bool UFNumberArray::IsDefault() const
{
    for (int i = 0; i < Size(); i++)
        if (!IsEqual(i, ufnumberarray->Default[i]))
            return false;
    return true;
}

void UFNumberArray::SetDefault()
{
    for (int i = 0; i < Size(); i++)
        ufnumberarray->Default[i] = ufnumberarray->Array[i];
    Event(uf_default_changed);
}

void UFNumberArray::Reset()
{
    bool changed = false;
    for (int i = 0; i < Size(); i++)
        changed |= ufnumberarray->SilentChange(this, i, ufnumberarray->Default[i]);
    if (changed)
        ufnumberarray->CallValueChangedEvent(this);
}

bool UFNumberArray::IsEqual(int index, double number) const
{
    if (index < 0 || index >= Size())
        Throw("index (%d) out of range 0..%d", index, Size() - 1);
    int newValue = floor(number / ufnumberarray->Accuracy + 0.5);
    int oldValue = floor(ufnumberarray->Array[index] / ufnumberarray->Accuracy + 0.5);
    return oldValue == newValue;
}

int UFNumberArray::Size() const
{
    return ufnumberarray->Size;
}

double UFNumberArray::Minimum() const
{
    return ufnumberarray->Minimum;
}

double UFNumberArray::Maximum() const
{
    return ufnumberarray->Maximum;
}

int UFNumberArray::AccuracyDigits() const
{
    return ufnumberarray->AccuracyDigits;
}
double UFNumberArray::Step() const
{
    return ufnumberarray->Step;
}

double UFNumberArray::Jump() const
{
    return ufnumberarray->Jump;
}

/***************************\
 * UFString implementation *
\***************************/

class _UFString : public _UFObject
{
public:
    char *Default;
    _UFString(UFName name) : _UFObject(name) { }
    ~_UFString() {
        g_free(Default);
    }
};

#define ufstring (static_cast<_UFString *>(ufobject))

UFString::UFString(UFName name, const char *defaultValue) :
    UFObject(new _UFString(name))
{
    ufstring->Default = g_strdup(defaultValue);
    ufstring->String = g_strdup(defaultValue);
}

void UFString::Set(const UFObject &object)
{
    if (this == &object) // Avoid self-assignment
        return;
    // We are comparing the strings ADDRESSes not values.
    if (Name() != object.Name())
        Throw("Object name mismatch with '%s'", object.Name());
    Set(object.StringValue());
}

void UFString::Set(const char *string)
{
    if (this->IsEqual(string))
        return;
    g_free(ufstring->String);
    ufstring->String = g_strdup(string);
    ufstring->CallValueChangedEvent(this);
}

bool UFString::IsDefault() const
{
    return this->IsEqual(ufstring->Default);
}

void UFString::SetDefault(const char *string)
{
    g_free(ufstring->Default);
    ufstring->Default = g_strdup(string);
    Event(uf_default_changed);
}

void UFString::SetDefault()
{
    SetDefault(ufstring->String);
}

void UFString::Reset()
{
    Set(ufstring->Default);
}

bool UFString::IsEqual(const char *string) const
{
    // If the pointers are equal, the strings are equal
    if (ufstring->String == string)
        return true;
    if (ufstring->String == NULL)
        return false;
    return strcmp(ufstring->String, string) == 0;
}

/**************************\
 * UFGroup implementation *
\**************************/

class _UFNameCompare
{
public:
    bool operator()(char const *a, char const *b) const {
        return strcmp(a, b) < 0;
    }
};
typedef std::map<const char *, UFObject *, _UFNameCompare> _UFGroupMap;
typedef std::pair<const char *, UFObject *> _UFObjectPair;
class _UFGroup : public _UFObject
{
public:
    _UFGroupMap Map;
    UFGroupList List;
    UFGroup *const This;
    bool GroupChanging;
    // Index and Default Index are only used by UFArray
    int Index;
    char *DefaultIndex;
    _UFGroup(UFGroup *that, UFName name, const char *label) :
        _UFObject(name), This(that), GroupChanging(false),
        Index(-1), DefaultIndex(NULL) {
        String = g_strdup(label);
    }
    bool Changing() const {
        if (Parent != NULL)
            return Parent->Changing();
        return GroupChanging;
    }
    void SetChanging(bool state) {
        if (Parent != NULL)
            Parent->SetChanging(state);
        else
            GroupChanging = state;
    }
};

// The following UFObject methods can only be implemented after _UFGroup
// is declared, since Parent is a _UFGroup object.
bool _UFObject::Changing() const
{
    if (Parent != NULL)
        return Parent->Changing();
    return false;
}

void _UFObject::SetChanging(bool state)
{
    if (Parent != NULL)
        Parent->SetChanging(state);
}

UFGroup &UFObject::Parent() const
{
    if (ufobject->Parent == NULL)
        Throw("UFObject has not parent");
    return *static_cast<_UFGroup*>(ufobject->Parent)->This;
}

#define ufgroup (static_cast<_UFGroup *>(ufobject))

// object is a <UFObject *> and generally not a <UFGroup *>.
// The cast to <UFGroup *> is needed for accessing ufobject.
#define _UFGROUP_PARENT(object) static_cast<UFGroup*>(object)->ufobject->Parent

UFGroup::UFGroup(UFName name, const char *label) :
    UFObject(new _UFGroup(this, name, label)) { }

UFGroup::~UFGroup()
{
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        _UFGROUP_PARENT(*iter) = NULL;
        delete *iter;
    }
    g_free(ufgroup->DefaultIndex);
}

static std::string _UFGroup_XML(const UFGroup &group, UFGroupList &list,
                                const char *indent, const char *attribute)
{
    if (group.IsDefault())
        return "";
    if (strcmp(attribute, "Index") == 0 && // If object is a UFArray and
            group.UFGroup::IsDefault()) { // all the array elements are default
        // Just print the value in a simple format.
        char *value = g_markup_escape_text(group.StringValue(), -1);
        std::string xml = (std::string)indent + "<" + group.Name() + ">" +
                          value + "</" + group.Name() + ">\n";
        return xml;
    }
    std::string xml = "";
    // For now, we don't want to surround the root XML with <[/]Image> tags.
    if (strlen(indent) != 0) {
        char *value = g_markup_escape_text(group.StringValue(), -1);
        if (value[0] == '\0') {
            xml += (std::string)indent + "<" + group.Name() + ">\n";
        } else {
            xml += (std::string)indent + "<" + group.Name() + " " +
                   attribute + "='" + value + "'>\n";
        }
        g_free(value);
    }
    char *newIndent = static_cast<char *>(g_alloca(strlen(indent) + 3));
    int i = 0;
    while (indent[i] != 0) {
        newIndent[i] = indent[i];
        i++;
    }
    newIndent[i + 0] = ' ';
    newIndent[i + 1] = ' ';
    newIndent[i + 2] = '\0';
    for (UFGroupList::iterator iter = list.begin(); iter != list.end(); iter++)
        xml += (*iter)->XML(newIndent);
    if (strlen(indent) != 0)
        xml  += (std::string)indent + "</" + group.Name() + ">\n";
    return xml;
}

std::string UFGroup::XML(const char *indent) const
{
    return _UFGroup_XML(*this, ufgroup->List, indent, "Label");
}

void UFGroup::Set(const UFObject &object)
{
    if (this == &object) // Avoid self-assignment
        return;
    // We are comparing the strings ADDRESSes not values.
    if (Name() != object.Name())
        Throw("Object name mismatch with '%s'", object.Name());
    const UFGroup &group = object;
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        if (group.Has((*iter)->Name()))
            (*iter)->Set(group[(*iter)->Name()]);
    }
}

void UFGroup::Set(const char * /*string*/)
{
    Throw("UFGroup does not support string values");
}

bool UFGroup::IsDefault() const
{
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        if (!(*iter)->IsDefault())
            return false;
    }
    return true;
}

void UFGroup::SetDefault()
{
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        (*iter)->SetDefault();
    }
    Event(uf_default_changed);
}

void UFGroup::Reset()
{
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        (*iter)->Reset();
    }
}

bool UFGroup::Has(UFName name) const
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(name);
    return iter != ufgroup->Map.end();
}

UFObject &UFGroup::operator[](UFName name)
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(name);
    if (iter == ufgroup->Map.end())
        Throw("No object with name '%s'", name); // out-of-range
    return *ufgroup->Map[name];
}

const UFObject &UFGroup::operator[](UFName name) const
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(name);
    if (iter == ufgroup->Map.end()) {
        Throw("No object with name '%s'", name); // out-of-range
    }
    return *ufgroup->Map[name];
}

const UFGroupList UFGroup::List() const
{
    return ufgroup->List;
}

UFGroup &UFGroup::operator<<(UFObject *object)
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(object->Name());
    if (iter != ufgroup->Map.end())
        Throw("index '%s' already exists", object->Name());
    ufgroup->Map.insert(_UFObjectPair(object->Name(), object));
    ufgroup->List.push_back(object);
    if (object->HasParent()) {
        // Remove object from its original group
        //_UFGroup *parent = static_cast<_UFGroup *>(object->ufobject->Parent);
        _UFGroup *parent = static_cast<_UFGroup *>(object->Parent().ufobject);
        parent->Map.erase(object->Name());
        for (UFGroupList::iterator iter = parent->List.begin();
                iter != parent->List.end(); iter++) {
            if (*iter == object) {
                parent->List.erase(iter);
                break;
            }
        }
    }
    _UFGROUP_PARENT(object) = ufgroup;
    Event(uf_element_added);
    return *this;
}

UFObject &UFGroup::Drop(UFName name)
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(name);
    if (iter == ufgroup->Map.end())
        Throw("index '%s' does not exists", name);
    UFObject *dropObject = (*iter).second;
    ufgroup->Map.erase(name);
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        if (*iter == dropObject) {
            ufgroup->List.erase(iter);
            break;
        }
    }
    _UFGROUP_PARENT(dropObject) = NULL;
    return *dropObject;
}

void UFGroup::Clear()
{
    for (_UFGroupMap::iterator iter = ufgroup->Map.begin();
            iter != ufgroup->Map.end(); iter++) {
        _UFGROUP_PARENT(iter->second) = NULL;
        delete iter->second;
    }
    ufgroup->Map.clear();
    ufgroup->List.clear();
}

// object is a <UFObject *> and generally not a <UFArray *>.
// The cast to <UFArray *> is needed for accessing ufobject.
#define _UFARRAY_PARENT(object) static_cast<UFArray *>(object)->ufobject->Parent

UFArray::UFArray(UFName name, const char *defaultIndex) :
    UFGroup(name, defaultIndex)
{
    ufgroup->DefaultIndex = g_strdup(defaultIndex);
}

std::string UFArray::XML(const char *indent) const
{
    return _UFGroup_XML(*this, ufgroup->List, indent, "Index");
}

void UFArray::Set(const UFObject &object)
{
    if (this == &object) // Avoid self-assignment
        return;
    // We are comparing the strings ADDRESSes not values.
    if (Name() != object.Name())
        Throw("Object name mismatch with '%s'", object.Name());
    const UFArray &array = object;
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++) {
        if (array.Has((*iter)->StringValue()))
            (*iter)->Set(array[(*iter)->StringValue()]);
    }
    Set(array.StringValue());
}

void UFArray::Set(const char *string)
{
    if (this->IsEqual(string))
        return;
    g_free(ufgroup->String);
    ufgroup->String = g_strdup(string);

    ufgroup->Index = -1;
    int i = 0;
    for (UFGroupList::iterator iter = ufgroup->List.begin();
            iter != ufgroup->List.end(); iter++, i++) {
        if (IsEqual((*iter)->StringValue())) {
            ufgroup->Index = i;
        }
    }
    ufgroup->CallValueChangedEvent(this);
}

const char *UFArray::StringValue() const
{
    return ufgroup->String;
}

bool UFArray::IsDefault() const
{
    if (!IsEqual(ufgroup->DefaultIndex))
        return false;
    return UFGroup::IsDefault();
}

void UFArray::SetDefault(const char *string)
{
    g_free(ufgroup->DefaultIndex);
    ufgroup->DefaultIndex = g_strdup(string);
    Event(uf_default_changed);
}

void UFArray::SetDefault()
{
    g_free(ufgroup->DefaultIndex);
    ufgroup->DefaultIndex = g_strdup(ufgroup->String);
    Event(uf_default_changed);
}

void UFArray::Reset()
{
    Set(ufgroup->DefaultIndex);
    UFGroup::Reset();
}

bool UFArray::SetIndex(int index)
{
    UFGroupList::iterator iter = ufgroup->List.begin();
    std::advance(iter, index);
    if (iter == ufgroup->List.end())
        return false;
    ufgroup->Index = index;
    Set((*iter)->StringValue());
    return true;
}

int UFArray::Index() const
{
    return ufgroup->Index;
}

bool UFArray::IsEqual(const char *string) const
{
    // If the pointers are equal, the strings are equal
    if (ufgroup->String == string)
        return true;
    if (ufgroup->String == NULL || string == NULL)
        return false;
    return strcmp(ufstring->String, string) == 0;
}

UFArray &UFArray::operator<<(UFObject *object)
{
    _UFGroupMap::iterator iter = ufgroup->Map.find(object->StringValue());
    if (iter != ufgroup->Map.end())
        Throw("index '%s' already exists", object->StringValue());
    ufgroup->Map.insert(_UFObjectPair(object->StringValue(), object));
    ufgroup->List.push_back(object);
    if (IsEqual(object->StringValue()))
        ufgroup->Index = ufgroup->List.size() - 1;
    if (object->HasParent()) {
        // Remove object from its original group.
        _UFGroup *parent = static_cast<UFArray *>(object)->ufobject->Parent;
        // We assume that the previous parent was also a UFArray.
        parent->Map.erase(object->StringValue());
        for (UFGroupList::iterator iter = parent->List.begin();
                iter != parent->List.end(); iter++) {
            if (*iter == object) {
                parent->List.erase(iter);
                break;
            }
        }
    }
    _UFARRAY_PARENT(object) = ufgroup;
    Event(uf_element_added);
    return *this;
}

UFException::UFException(std::string &Message) :
    std::runtime_error(Message.c_str()) { }

/******************************\
 * C interface implementation *
\******************************/

extern "C" {

    UFName ufobject_name(UFObject *object)
    {
        return object->Name();
    }

    UFObject *ufobject_delete(UFObject *object)
    {
        delete object;
        return NULL;
    }

    UFObject *ufobject_parent(UFObject *object)
    {
        try {
            return &object->Parent();
        } catch (UFException &e) {
            object->Message(e.what());
            return NULL;
        }
    }

    const char *ufobject_string_value(UFObject *object)
    {
        return object->StringValue();
    }

    UFBoolean ufobject_set_string(UFObject *object, const char *string)
    {
        try {
            object->Set(string);
            return true;
        } catch (UFException &e) {
            // Could be an string-is-not-a-number exception.
            // Caller should handle the message.
            return false;
        }
    }

    UFBoolean ufobject_copy(UFObject *destination, UFObject *source)
    {
        try {
            destination->Set(*source);
            return true;
        } catch (UFException &e) {
            destination->Message(e.what());
            return false;
        }
    }

    char *ufobject_xml(UFObject *object, const char *indent)
    {
        std::string xml = object->XML(indent);
        return g_strdup(xml.c_str());
    }

    void *ufobject_user_data(UFObject *object)
    {
        return object->UserData();
    }

    void ufobject_set_user_data(UFObject *object, void *user_data)
    {
        object->SetUserData(user_data);
    }

    void ufobject_set_changed_event_handle(UFObject *object, UFEventHandle *handle)
    {
        object->SetEventHandle(handle);
    }

    UFBoolean ufobject_is_default(UFObject *object)
    {
        return object->IsDefault();
    }

    void ufobject_set_default(UFObject *object)
    {
        object->SetDefault();
    }

    double ufnumber_value(UFObject *object)
    {
        try {
            return dynamic_cast<UFNumber &>(*object).DoubleValue();
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    UFBoolean ufnumber_set(UFObject *object, double number)
    {
        try {
            dynamic_cast<UFNumber *>(object)->Set(number);
            return true;
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

    double ufnumber_array_value(UFObject *object, int index)
    {
        try {
            return dynamic_cast<UFNumberArray &>(*object).DoubleValue(index);
        } catch (UFException &e) {
            object->Message(e.what());
            return std::numeric_limits<double>::quiet_NaN();
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    UFBoolean ufnumber_array_set(UFObject *object, const double array[])
    {
        try {
            dynamic_cast<UFNumberArray &>(*object).Set(array);
            return true;
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

    UFBoolean ufstring_is_equal(UFObject *object, const char *string)
    {
        try {
            return dynamic_cast<UFString *>(object)->IsEqual(string);
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

    UFBoolean ufgroup_has(UFObject *object, UFName name)
    {
        try {
            return dynamic_cast<UFGroup *>(object)->Has(name);
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

    UFObject *ufgroup_element(UFObject *object, UFName name)
    {
        try {
            UFObject &element = dynamic_cast<UFGroup &>(*object)[name];
            return &element;
        } catch (UFException &e) {
            object->Message(e.what());
            return NULL;
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return NULL;
        }
    }

    UFBoolean ufgroup_add(UFObject *group, UFObject *object)
    {
        try {
            dynamic_cast<UFGroup &>(*group) << object;
            return true;
        } catch (UFException &e) {
            group->Message(e.what());
            return false;
        } catch (std::bad_cast &e) {
            group->Message(e.what());
            return false;
        }
    }

    UFObject *ufgroup_drop(UFObject *group, UFName name)
    {
        try {
            return &dynamic_cast<UFGroup *>(group)->Drop(name);
        } catch (UFException &e) {
            group->Message(e.what());
            return NULL;
        } catch (std::bad_cast &e) {
            group->Message(e.what());
            return NULL;
        }
    }

    UFBoolean ufarray_set_index(UFObject *object, int index)
    {
        try {
            return dynamic_cast<UFArray *>(object)->SetIndex(index);
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

    int ufarray_index(UFObject *object)
    {
        try {
            return dynamic_cast<UFArray *>(object)->Index();
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return -2;
        }
    }

    UFBoolean ufarray_is_equal(UFObject *object, const char *string)
    {
        try {
            return dynamic_cast<UFArray *>(object)->IsEqual(string);
        } catch (std::bad_cast &e) {
            object->Message(e.what());
            return false;
        }
    }

} // extern "C"
