/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "rs-facebook-client-param.h"

typedef struct {
	gchar *name;
	gchar *value;
} ParamPair;

static ParamPair *param_pair_new(gchar *name, gchar *value);
static void param_pair_free(gpointer data, gpointer user_data);
static gint param_pair_cmp(gconstpointer a, gconstpointer b);

static ParamPair *
param_pair_new(gchar *name, gchar *value)
{
	ParamPair *pair = g_new(ParamPair, 1);
	pair->name = name;
	pair->value = value;

	return pair;
}

static void
param_pair_free(gpointer data, gpointer user_data)
{
	ParamPair *pair = (ParamPair *) data;

	g_free(pair->name);
	g_free(pair->value);
	g_free(pair);
}

static gint
param_pair_cmp(gconstpointer a, gconstpointer b)
{
	ParamPair *pair_a = (ParamPair *) a;
	ParamPair *pair_b = (ParamPair *) b;

	if (pair_a == pair_b)
		return 0;

	if (!pair_a)
		return 1;

	if (!pair_b)
		return -1;

	return g_strcmp0(pair_a->name, pair_b->name);
}

G_DEFINE_TYPE(RSFacebookClientParam, rs_facebook_client_param, G_TYPE_OBJECT)

static void
rs_facebook_client_param_finalize(GObject *object)
{
	RSFacebookClientParam *param = RS_FACEBOOK_CLIENT_PARAM(object);

	g_list_foreach(param->params, param_pair_free, NULL);

	G_OBJECT_CLASS(rs_facebook_client_param_parent_class)->finalize(object);
}

static void
rs_facebook_client_param_class_init(RSFacebookClientParamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = rs_facebook_client_param_finalize;
}

static void
rs_facebook_client_param_init(RSFacebookClientParam *self)
{
}

/**
 * Initialize a new RSFacebookClientParam
 * @return Anew RSFacebookClientParam
 */
RSFacebookClientParam *
rs_facebook_client_param_new(void)
{
	return g_object_new(RS_TYPE_FACEBOOK_CLIENT_PARAM, NULL);
}

/**
 * Add a string argument to a RSFacebookClientParam
 * @param param A RSFacebookClientParam
 * @param name The name of the parameter
 * @param value The value of the parameter
 */
void
rs_facebook_client_param_add_string(RSFacebookClientParam *param, const gchar *name, const gchar *value)
{
	g_assert(RS_IS_FACEBOOK_CLIENT_PARAM(param));

	ParamPair *pair = param_pair_new(g_strdup(name), g_strdup(value));
	param->params = g_list_append(param->params, pair);
}

/**
 * Add a string argument to a RSFacebookClientParam
 * @param param A RSFacebookClientParam
 * @param name The name of the parameter
 * @param value The value of the parameter
 */
void
rs_facebook_client_param_add_integer(RSFacebookClientParam *param, const gchar *name, const gint value)
{
	g_assert(RS_IS_FACEBOOK_CLIENT_PARAM(param));

	ParamPair *pair = param_pair_new(g_strdup(name), g_strdup_printf("%d", value));
	param->params = g_list_append(param->params, pair);
}

/**
 * Get the complete POST string to use for a Facebook request including signature
 * @param param A RSFacebookClientParam
 * @param secret The secret provided by Facebook
 * @param boundary A string to use as HTTP boundary
 * @param length If non-NULL, will store the length of the post string returned
 * @return A newly allocated POST-string, this must be freed with g_free()
 */
gchar *
rs_facebook_client_param_get_post(RSFacebookClientParam *param, const gchar *secret, const gchar *boundary, gint *length)
{
	gchar *post;
	gchar *signature;
	GString *str;
	GString *attachment = NULL;
	GList *node;

	g_assert(RS_IS_FACEBOOK_CLIENT_PARAM(param));
	g_assert(secret != NULL);
	g_assert(boundary != NULL);

	/* Facebook requires parameter list to be sorted, we'll do that */
	param->params = g_list_sort(param->params, param_pair_cmp);

	/* Build signature */
	str = g_string_sized_new(10240);
	for(node = g_list_first(param->params) ; node != NULL; node = g_list_next(node))
	{
		ParamPair *pair = node->data;
		g_string_append_printf(str, "%s=%s", pair->name, pair->value);
	}
	g_string_append_printf(str, "%s", secret);
	signature = g_compute_checksum_for_string(G_CHECKSUM_MD5, str->str, str->len);
	g_string_free(str, TRUE);

	rs_facebook_client_param_add_string(param, "sig", signature);
	g_free(signature);

	/* Build complete post */
	str = g_string_sized_new(10240);
	for(node = g_list_first(param->params) ; node != NULL; node = g_list_next(node))
	{
		ParamPair *pair = node->data;

		/* If we encounter filename, we prepare data to be added last */
		if (g_strcmp0(pair->name, "filename") == 0)
		{
			gchar *contents;
			gsize length;
			if (g_file_get_contents(pair->value, &contents, &length, NULL))
			{
				if (!length)
					g_warning("You must use the length argument, if you attaches a file");
				/* We try toallocate everything we need up front */
				attachment = g_string_sized_new(length + 200);
				g_string_append_printf(attachment, "--%s\r\n", boundary);
				g_string_append_printf(attachment, "Content-Disposition: form-data; filename=%s\r\n", pair->value);
				g_string_append_printf(attachment, "Content-Type: image/jpg\r\n\r\n");
				attachment = g_string_append_len(attachment, contents, length);
				g_string_append_printf(attachment, "\r\n--%s\r\n", boundary);

				g_free(contents);
			}
		}
		g_string_append_printf(str, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n", boundary, pair->name, pair->value);
	}

	if (attachment)
	{
		str = g_string_append_len(str, attachment->str, attachment->len);
		g_string_free(attachment, TRUE);
	}

	/* Assign length if requested */
	if (length)
		*length = str->len;

	post = str->str;
	g_string_free(str, FALSE);

	return post;
}
