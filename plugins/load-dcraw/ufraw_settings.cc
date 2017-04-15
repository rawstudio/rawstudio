/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_settings.cc - define all UFObject settings.
 * Copyright 2004-2015 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ufraw.h"
#include "dcraw_api.h"
#include <glib/gi18n.h>
#include <string.h>
#include <assert.h>

namespace UFRaw
{

// Shortcut for UFString definitions
#define UF_STRING(Class, Name, String, ...) \
    extern "C" { UFName Name = String; } \
    class Class : public UFString { \
    public: \
	Class() : UFString(Name, __VA_ARGS__) { } \
    }
// We could have also used a template:
/*
template <const char *name>
    class UF_STRING : public UFString {
    public:
	UF_STRING() : UFString(name) { }
    };

typedef UF_STRING<ufCameraMake> UFCameraMake;
*/

// Shortcut for UFNumber definitions
#define UF_NUMBER(Class, Name, String, ...) \
    extern "C" { UFName Name = String; } \
    class Class : public UFNumber { \
    public: \
	Class() : UFNumber(Name, __VA_ARGS__) { } \
    }

// ufRawImage is short for 'raw image processing parameters'.
extern "C" {
    UFName ufRawImage = "Image";
}
// Common class for Image and CommandLineImage
class ImageCommon : public UFGroup
{
private:
public:
    // uf should be private
    ufraw_data *uf;
    ImageCommon() : UFGroup(ufRawImage), uf(NULL) { }
};

class Image : public ImageCommon
{
private:
public:
    explicit Image(UFObject *root = NULL);
    void SetUFRawData(ufraw_data *data);
    static ufraw_data *UFRawData(UFObject *object)
    {
        if (object->Name() == ufRawImage)
            return dynamic_cast<Image *>(object)->uf;
        if (!object->HasParent())
            return NULL;
        return Image::UFRawData(&object->Parent());
    }
    void SetWB(const char *mode = NULL);
    void Message(const char *Format, ...) const
    {
        if (Format == NULL)
            return;
        va_list ap;
        va_start(ap, Format);
        char *message = g_strdup_vprintf(Format, ap);
        va_end(ap);
        ufraw_message(UFRAW_ERROR, "%s: %s\n", Name(), message);
        g_free(message);
    }
};

static Image &ParentImage(UFObject *obj)
{
    return static_cast<Image &>(obj->Parent());
}

//UF_STRING(CameraMake, ufCameraMake, "Make", "");

extern "C" {
    UFName ufWB = "WB";
}
extern "C" {
    UFName ufPreset = "Preset";
}
class WB : public UFArray
{
public:
    WB() : UFArray(ufWB, uf_camera_wb) { }
    void Event(UFEventType type)
    {
        // spot_wb is a temporary value, that would be changed in SetWB()
        if (!this->IsEqual(uf_spot_wb))
            UFObject::Event(type);
    }
    void OriginalValueChangedEvent()
    {
        /* Keep compatibility with old numbers from ufraw-0.6 */
        int i;
        if (strlen(StringValue()) <= 2 &&
                sscanf(StringValue(), "%d", &i) == 1) {
            switch (i) {
                case -1:
                    Set(uf_spot_wb);
                    break;
                case 0:
                    Set(uf_manual_wb);
                    break;
                case 1:
                    Set(uf_camera_wb);
                    break;
                case 2:
                    Set(uf_auto_wb);
                    break;
                case 3:
                    Set("Incandescent");
                    break;
                case 4:
                    Set("Fluorescent");
                    break;
                case 5:
                    Set("Direct sunlight");
                    break;
                case 6:
                    Set("Flash");
                    break;
                case 7:
                    Set("Cloudy");
                    break;
                case 8:
                    Set("Shade");
                    break;
                default:
                    Set("");
            }
        }
        if (HasParent())
            ParentImage(this).SetWB();
    }
    // Use the original XML format instead of UFArray's format.
    // Output XML block even if IsDefault().
    std::string XML(const char *indent) const
    {
        char *value = g_markup_escape_text(StringValue(), -1);
        std::string str = (std::string)indent +
                          "<" + Name() + ">" + value + "</" + Name() + ">\n";
        g_free(value);
        return str;
    }
};

extern "C" {
    UFName ufWBFineTuning = "WBFineTuning";
}
class WBFineTuning : public UFNumber
{
public:
    WBFineTuning() : UFNumber(ufWBFineTuning, -9, 9, 0, 0, 1, 1) { }
    void OriginalValueChangedEvent()
    {
        if (!HasParent())
            return;
        UFArray &wb = ParentImage(this)[ufWB];
        if (wb.IsEqual(uf_auto_wb) || wb.IsEqual(uf_camera_wb))
            /* Prevent recalculation of Camera/Auto WB on WBTuning events */
            Set(0.0);
        else
            ParentImage(this).SetWB();
    }
    // Output XML block even if IsDefault().
    std::string XML(const char *indent) const
    {
        char *value = g_markup_escape_text(StringValue(), -1);
        std::string str = (std::string)indent +
                          "<" + Name() + ">" + value + "</" + Name() + ">\n";
        g_free(value);
        return str;
    }
};

extern "C" {
    UFName ufTemperature = "Temperature";
}
class Temperature : public UFNumber
{
public:
    Temperature() : UFNumber(ufTemperature, 2000, 23000, 6500, 0, 50, 200) { }
    void OriginalValueChangedEvent()
    {
        if (HasParent())
            ParentImage(this).SetWB(uf_manual_wb);
    }
};

extern "C" {
    UFName ufGreen = "Green";
}
class Green : public UFNumber
{
public:
    Green() : UFNumber(ufGreen, 0.2, 2.5, 1.0, 3, 0.01, 0.05) { };
    void OriginalValueChangedEvent()
    {
        if (HasParent())
            ParentImage(this).SetWB(uf_manual_wb);
    }
};

extern "C" {
    UFName ufChannelMultipliers = "ChannelMultipliers";
}
class ChannelMultipliers : public UFNumberArray
{
public:
    ChannelMultipliers() : UFNumberArray(ufChannelMultipliers, 4,
                                             0.010, 99.000, 1.0, 3, 0.001,
                                             0.001) { };
    void Event(UFEventType type)
    {
        if (type != uf_value_changed)
            return UFObject::Event(type);
        if (!HasParent())
            return UFObject::Event(type);
        ufraw_data *uf = Image::UFRawData(this);
        if (uf == NULL)
            return UFObject::Event(type);
        /* Normalize chanMul so that min(chanMul) will be 1.0 */
        double min = Maximum();
        for (int c = 0; c < uf->colors; c++)
            if (DoubleValue(c) < min)
                min = DoubleValue(c);
        assert(min > 0.0);
        double chanMulArray[4] = { 1.0, 1.0, 1.0, 1.0 };
        for (int c = 0; c < uf->colors; c++)
            chanMulArray[c] = DoubleValue(c) / min;
        Set(chanMulArray);

        if (uf->conf->autoExposure == enabled_state)
            uf->conf->autoExposure = apply_state;
        if (uf->conf->autoBlack == enabled_state)
            uf->conf->autoBlack = apply_state;

        UFObject::Event(type);
    }
    void OriginalValueChangedEvent()
    {
        if (HasParent())
            ParentImage(this).SetWB(uf_spot_wb);
    }
    // Output XML block even if IsDefault().
    std::string XML(const char *indent) const
    {
        std::string str = "";
        char num[10];
        for (int i = 0; i < Size(); i++) {
            g_snprintf(num, 10, "%.6lf", DoubleValue(i));
            str += num;
            if (i < Size() - 1)
                str += " ";
        }
        char *value = g_markup_escape_text(str.c_str(), -1);
        str = (std::string)indent +
              "<" + Name() + ">" + value + "</" + Name() + ">\n";
        g_free(value);
        return str;
    }
};

extern "C" {
    UFName ufLensfunAuto = "LensfunAuto";
}
class LensfunAuto : public UFString
{
public:
    LensfunAuto() : UFString(ufLensfunAuto, "yes") { }
    void OriginalValueChangedEvent()
    {
        if (!HasParent())
            return;
        if (IsEqual("auto")) {
            Set("yes");
            return;
        }
        if (IsEqual("none")) {
            Set("no");
            return;
        }
        if (!IsEqual("yes") && !IsEqual("no"))
            Throw("Invalid value '%s'", StringValue());
#ifdef HAVE_LENSFUN
        if (!Parent().Has(ufLensfun))
            return;
        if (IsEqual("yes"))
            ufraw_lensfun_init(&Parent()[ufLensfun], TRUE);
#endif
    }
};

Image::Image(UFObject *root) : ImageCommon()
{
    *this
            << new WB
            << new WBFineTuning
            << new Temperature
            << new Green
            << new ChannelMultipliers
            ;
#ifdef HAVE_LENSFUN
    *this << new LensfunAuto;
    if (root == NULL || root->Name() != ufRawResources)
        *this << ufraw_lensfun_new(); // Lensfun data is not saved to .ufrawrc
#else
    (void)root;
#endif
}

void Image::SetWB(const char *mode)
{
    UFArray &wb = (*this)[ufWB];
    if (wb.IsEqual(uf_manual_wb) || wb.IsEqual(uf_camera_wb) ||
            wb.IsEqual(uf_auto_wb) || wb.IsEqual(uf_spot_wb)) {
        if (!Has(ufWBFineTuning))
            *this << new WBFineTuning;
        UFNumber &wbTuning = (*this)[ufWBFineTuning];
        wbTuning.Set(0.0);
    }
    // While loading rc/cmd/conf data we do not want to alter the raw data.
    if (uf == NULL)
        return;
    if (uf->rgbMax == 0) { // Raw file was not loaded yet.
        if (!wb.IsEqual(uf_manual_wb))
            uf->WBDirty = true; // ChannelMultipliers should be calculated later
        return;
    }
    if (mode != NULL)
        wb.Set(mode);
    ufraw_set_wb(uf);
    if (wb.IsEqual(uf_spot_wb))
        wb.Set(uf_manual_wb);
}

void Image::SetUFRawData(ufraw_data *data)
{
    uf = data;
    if (uf == NULL)
        return;

    dcraw_data *raw = static_cast<dcraw_data *>(uf->raw);
    if (strcasecmp(uf->conf->make, raw->make) != 0 ||
            strcmp(uf->conf->model, raw->model) != 0)
        uf->WBDirty = TRUE; // Re-calculate channel multipliers.
    if (uf->LoadingID)
        uf->WBDirty = TRUE; // Re-calculate channel multipliers.
    g_strlcpy(uf->conf->make, raw->make, max_name);
    g_strlcpy(uf->conf->model, raw->model, max_name);
    if (!uf->LoadingID)
        uf->WBDirty = TRUE; // Re-calculate channel multipliers.

    const wb_data *lastPreset = NULL;
    uf->wb_presets_make_model_match = FALSE;
    char model[max_name];
    if (strcasecmp(uf->conf->make, "Minolta") == 0 &&
            (strncmp(uf->conf->model, "ALPHA", 5) == 0 ||
             strncmp(uf->conf->model, "MAXXUM", 6) == 0)) {
        /* Canonize Minolta model names (copied from dcraw) */
        g_snprintf(model, max_name, "DYNAX %s",
                   uf->conf->model + 6 + (uf->conf->model[0] == 'M'));
    } else {
        g_strlcpy(model, uf->conf->model, max_name);
    }
    UFArray &wb = (*this)[ufWB];
    for (int i = 0; i < wb_preset_count; i++) {
        if (strcmp(wb_preset[i].make, "") == 0) {
            /* Common presets */
            if (strcmp(wb_preset[i].name, uf_camera_wb) == 0) {
                // Get the camera's presets.
                int status = dcraw_set_color_scale(raw, TRUE);
                // Failure means that dcraw does not support this model.
                if (status != DCRAW_SUCCESS) {
                    if (wb.IsEqual(uf_camera_wb)) {
                        ufraw_message(UFRAW_SET_LOG,
                                      _("Cannot use camera white balance, "
                                        "reverting to auto white balance.\n"));
                        wb.Set(uf_auto_wb);
                    }
                    continue;
                }
            }
            wb << new UFString(ufPreset, wb_preset[i].name);
        } else if (strcasecmp(wb_preset[i].make, uf->conf->make) == 0 &&
                   strcmp(wb_preset[i].model, model) == 0) {
            /* Camera specific presets */
            uf->wb_presets_make_model_match = TRUE;
            if (lastPreset == NULL ||
                    strcmp(wb_preset[i].name, lastPreset->name) != 0) {
                wb << new UFString(ufPreset, wb_preset[i].name);
            }
            lastPreset = &wb_preset[i];
        }
    }
}

extern "C" {
    UFName ufRawResources = "Resources";
}
class Resources : public UFGroup
{
public:
    Resources(): UFGroup(ufRawResources)
    {
        *this << new Image(this);
    }
};

class CommandLineImage : public ImageCommon
{
public:
    CommandLineImage(): ImageCommon() { }
    void Event(UFEventType type)
    {
        if (type != uf_element_added)
            return UFObject::Event(type);
        if (Has(ufTemperature) || Has(ufGreen)) {
            if (Has(ufWB)) {
                UFArray &wb = (*this)[ufWB];
                if (!wb.IsEqual(uf_manual_wb) && !wb.IsEqual(uf_camera_wb)) {
                    ufraw_message(UFRAW_WARNING,
                                  _("--temperature and --green options override "
                                    "the --wb=%s option."), wb.StringValue());
                }
            } else {
                *this << new WB;
            }
            (*this)[ufWB].Set(uf_manual_wb);
        } else {
            if (Has(ufWB)) {
                // We don't have green or temperature so this must be from --wb
                UFArray &wb = (*this)[ufWB];
                if (wb.IsEqual(uf_auto_wb) || wb.IsEqual(uf_camera_wb))
                    return UFObject::Event(type);
                if (wb.IsEqual("camera"))
                    wb.Set(uf_camera_wb);
                else if (wb.IsEqual("auto"))
                    wb.Set(uf_auto_wb);
                else
                    Throw(_("'%s' is not a valid white balance setting."),
                          wb.StringValue());
            }
        }
        return UFObject::Event(type);
    }
};

extern "C" {
    UFName ufCommandLine = "CommandLine";
}
class CommandLine : public UFGroup
{
public:
    CommandLine(): UFGroup(ufCommandLine)
    {
        *this << new CommandLineImage;
    }
    void Message(const char *Format, ...) const
    {
        if (Format == NULL)
            return;
        va_list ap;
        va_start(ap, Format);
        char *message = g_strdup_vprintf(Format, ap);
        va_end(ap);
        ufraw_message(UFRAW_ERROR, "%s: %s\n", Name(), message);
        g_free(message);
    }
};

} // namespace UFRaw

extern "C" {

    UFObject *ufraw_image_new()
    {
        return new UFRaw::Image;
    }

    void ufraw_image_set_data(UFObject *obj, struct ufraw_struct *uf)
    {
        dynamic_cast<UFRaw::Image *>(obj)->SetUFRawData(uf);
    }

    struct ufraw_struct *ufraw_image_get_data(UFObject *obj)
    {
        return UFRaw::Image::UFRawData(obj);
    }

    UFObject *ufraw_resources_new()
    {
        return new UFRaw::Resources;
    }

    UFObject *ufraw_command_line_new()
    {
        return new UFRaw::CommandLine;
    }

} // extern "C"
