/*
 * This addon adds Tiled map support to the Allegro game library.
 * Copyright (c) 2012-2014 Damien Radtke - www.damienradtke.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 *
 *                               ---
 *
 * Methods for reading and parsing map files.
 */

#include <allegro5/allegro.h>
#include "parser.h"
#include "common.h"
/*
 * Small workaround for Allegro's list creation.
 * _al_list_create_static() doesn't work for lists of size 0.
static inline _AL_LIST *create_list(size_t capacity)
{
	if (capacity == 0) {
		return _al_list_create();
	} else {
		return _al_list_create_static(capacity);
	}
}
*/

/*
 * Decodes map data from a <data> node
 */
static void decode_layer_data(xmlNode *data_node, ALLEGRO_MAP_LAYER *layer)
{
	char *str = strstrip((char *)data_node->children->content);
	int datalen = layer->width * layer->height;
	layer->data = (char *)al_calloc(datalen, sizeof(char));

	char *encoding = get_xml_attribute(data_node, "encoding");
	if (!encoding) {
		int i = 0;
		SList *tiles = get_children_for_name(data_node, "tile");
		SList *tile_item = tiles;
		while (tile_item) {
			xmlNode *tile_node = (xmlNode*)tile_item->data;
			tile_item = slist_next(tile_item);
			char *gid = get_xml_attribute(tile_node, "gid");
			layer->data[i] = atoi(gid);
			i++;
		}

		slist_free(tiles);
	}
	else if (streq(encoding, "base64")) {
		// decompress
		size_t rawlen;
		unsigned char *rawdata = base64_decode(str, &rawlen);

		// check the compression
		char *compression = get_xml_attribute(data_node, "compression");
		if (compression != NULL) {
			if(!streq(compression, "zlib") && !streq(compression, "gzip")) {
				fprintf(stderr, "Error: unknown compression format '%s'\n", compression);
				return;
			}

			// set up files used by zlib to decompress the data
			ALLEGRO_PATH *srcpath;
			ALLEGRO_FILE *datasrc = al_make_temp_file("XXXXXX", &srcpath);
			al_fwrite(datasrc, rawdata, rawlen);
			al_fseek(datasrc, 0, ALLEGRO_SEEK_SET);
			//al_fclose(datasrc);
			//datasrc = al_fopen(al_path_cstr(srcpath, ALLEGRO_NATIVE_PATH_SEP), "rb");
			ALLEGRO_FILE *datadest = al_make_temp_file("XXXXXX", NULL);

			// decompress and print an error if it failed
			int status = inf(datasrc, datadest);
			if (status)
				zerr(status);

			// flush data and get the file length
			al_fflush(datadest);
			int len = al_fsize(datadest);

			// read in the file
			al_fseek(datadest, 0, ALLEGRO_SEEK_SET);
			char *data = (char *)al_calloc(len, sizeof(char));
			if (al_fread(datadest, data, len) != len) {
				fprintf(stderr, "Error: failed to read in map data\n");
				return;
			}

			// every tile id takes 4 bytes
			int i;
			for (i = 0; i<len; i += 4) {
				int tileid = 0;
				tileid |= data[i];
				tileid |= data[i+1] << 8;
				tileid |= data[i+2] << 16;
				tileid |= data[i+3] << 24;

				layer->data[i/4] = tileid;
			}
			/*	printf("layer dimensions: %dx%d, data length = %d\n",
						layer->width, layer->height, len); */

			al_destroy_path(srcpath);
			al_fclose(datasrc);
			al_fclose(datadest);
			al_free(data);
		}
		else {
			// TODO: verify that this still works
			int i;
			for (i = 0; i<rawlen; i += 4) {
				int tileid = 0;
				tileid |= rawdata[i];
				tileid |= rawdata[i+1] << 8;
				tileid |= rawdata[i+2] << 16;
				tileid |= rawdata[i+3] << 24;

				layer->data[i/4] = tileid;
			}
		}

		al_free(rawdata);
	}
	else if (streq(encoding, "csv")) {
		int i;
		for (i = 0; i<datalen; i++) {
			char *id = strtok((i == 0 ? str : NULL), ",");
			layer->data[i] = atoi(id);
		}
	}
	else {
		fprintf(stderr, "Error: unknown encoding format '%s'\n", encoding);
	}
}

/*
 * After all the tiles have been parsed out of their tilesets,
 * create the map's global list of tiles.
 */
static void cache_tile_list(ALLEGRO_MAP *map)
{
	map->tiles = rb_tree_new((cmp_func)intcmp, free, NULL);
	SList *tileset_item = map->tilesets;

	while (tileset_item != NULL) {
		ALLEGRO_MAP_TILESET *tileset = (ALLEGRO_MAP_TILESET*)tileset_item->data;
		tileset_item = slist_next(tileset_item);
		SList *tile_item = tileset->tiles;
		while (tile_item != NULL) {
			ALLEGRO_MAP_TILE *tile = (ALLEGRO_MAP_TILE*)tile_item->data;
			tile_item = slist_next(tile_item);
			// associate the tile's id with its ALLEGRO_TILE struct
			rb_tree_insert(map->tiles, intdup(tile->id), tile);
		}
	}
}

/*
 * Parse a <properties> node into a list of property objects.
 */
static RBTree *parse_properties(xmlNode *node)
{
	RBTree *props = rb_tree_new((cmp_func)strcmp, free, free);

	xmlNode *properties_node = get_first_child_for_name(node, "properties");
	if (!properties_node) {
		return props;
	}

	SList *properties_list = get_children_for_name(properties_node, "property");
	SList *property_item = properties_list;
	while (property_item) {
		xmlNode *property_node = (xmlNode*)property_item->data;
		property_item = slist_next(property_item);

		char *name = n_strdup(get_xml_attribute(property_node, "name"));
		char *value = get_xml_attribute(property_node, "value");
		if (!value) {
			value = (char*)xmlNodeGetContent(property_node);
		}

		value = n_strdup(value);
		rb_tree_insert(props, name, value);
	}

	slist_free(properties_list);
	return props;
}

/*
 * Parses a map file
 * Given the path to a map file, returns a new map struct
 * The struct must be freed once it's done being used
 */
ALLEGRO_MAP *al_open_map(const char *dir, const char *filename)
{
	xmlDoc *doc;
	xmlNode *root;
	ALLEGRO_MAP *map;

	unsigned i, j;

	ALLEGRO_PATH *cwd = al_get_standard_path(ALLEGRO_RESOURCES_PATH);
	ALLEGRO_PATH *resources = al_clone_path(cwd);
	ALLEGRO_PATH *maps = al_create_path(dir);

	al_join_paths(resources, maps);
	if (!al_change_directory(al_path_cstr(resources, ALLEGRO_NATIVE_PATH_SEP))) {
		fprintf(stderr, "Error: failed to change directory in al_parse_map().");
	}

	al_destroy_path(resources);
	al_destroy_path(maps);

	// Read in the data file
	doc = xmlReadFile(filename, NULL, 0);
	if (!doc) {
		fprintf(stderr, "Error: failed to parse map data: %s\n", filename);
		return NULL;
	}

	// Get the root element, <map>
	root = xmlDocGetRootElement(doc);

	// Get some basic info
	map = MALLOC(ALLEGRO_MAP);
	map->width = atoi(get_xml_attribute(root, "width"));
	map->height = atoi(get_xml_attribute(root, "height"));
	map->tile_width = atoi(get_xml_attribute(root, "tilewidth"));
	map->tile_height = atoi(get_xml_attribute(root, "tileheight"));
	map->orientation = n_strdup(get_xml_attribute(root, "orientation"));
	map->tile_layer_count = 0;
	map->object_layer_count = 0;

	// Get the tilesets
	SList *tilesets = get_children_for_name(root, "tileset");
	map->tilesets = NULL;

	SList *tileset_item = tilesets;
	while (tileset_item) {
		xmlNode *tileset_node = (xmlNode*)tileset_item->data;
		tileset_item = slist_next(tileset_item);

		ALLEGRO_MAP_TILESET *tileset = MALLOC(ALLEGRO_MAP_TILESET);
		tileset->firstgid = atoi(get_xml_attribute(tileset_node, "firstgid"));
		tileset->tilewidth = atoi(get_xml_attribute(tileset_node, "tilewidth"));
		tileset->tileheight = atoi(get_xml_attribute(tileset_node, "tileheight"));
		tileset->name = n_strdup(get_xml_attribute(tileset_node, "name"));

		// Get this tileset's image
		xmlNode *image_node = get_first_child_for_name(tileset_node, "image");
		tileset->width = atoi(get_xml_attribute(image_node, "width"));
		tileset->height = atoi(get_xml_attribute(image_node, "height"));
		tileset->source = n_strdup(get_xml_attribute(image_node, "source"));
		tileset->bitmap = al_load_bitmap(tileset->source);

		// Get this tileset's tiles
		SList *tiles = get_children_for_name(tileset_node, "tile");
		tileset->tiles = NULL;

		SList *tile_item = tiles;
		while (tile_item) {
			xmlNode *tile_node = (xmlNode*)tile_item->data;
			tile_item = slist_next(tile_item);

			ALLEGRO_MAP_TILE *tile = MALLOC(ALLEGRO_MAP_TILE);
			tile->id = tileset->firstgid + atoi(get_xml_attribute(tile_node, "id"));
			tile->tileset = tileset;
			tile->bitmap = NULL;

			// Get this tile's properties
			tile->properties = parse_properties(tile_node);

			// TODO: add a destructor
			tileset->tiles = slist_prepend(tileset->tiles, tile);
		}

		slist_free(tiles);
		//tileset->tiles = slist_reverse(tileset->tiles);

		// TODO: add a destructor
		map->tilesets = slist_prepend(map->tilesets, tileset);
	}

	slist_free(tilesets);
	//map->tilesets = slist_reverse(map->tilesets);

	// Create the map's master list of tiles
	cache_tile_list(map);

	// Get the layers
	SList *layers = get_children_for_either_name(root, "layer", "objectgroup");
	map->layers = NULL;

	SList *layer_item = layers;
	while (layer_item) {
		xmlNode *layer_node = (xmlNode*)layer_item->data;
		layer_item = slist_next(layer_item);

		ALLEGRO_MAP_LAYER *layer = MALLOC(ALLEGRO_MAP_LAYER);
		layer->name = n_strdup(get_xml_attribute(layer_node, "name"));
		layer->properties = parse_properties(layer_node);

		char *layer_visible = get_xml_attribute(layer_node, "visible");
		layer->visible = (layer_visible != NULL ? atoi(layer_visible) : 1);

		char *layer_opacity = get_xml_attribute(layer_node, "opacity");
		layer->opacity = (layer_opacity != NULL ? atof(layer_opacity) : 1.0);

		if (streq((const char*)layer_node->name, "layer")) {
			layer->type = TILE_LAYER;
			layer->width = atoi(get_xml_attribute(layer_node, "width"));
			layer->height = atoi(get_xml_attribute(layer_node, "height"));
			decode_layer_data(get_first_child_for_name(layer_node, "data"), layer);

			// Create any missing tile objects
			for (i = 0; i<layer->height; i++) {
				for (j = 0; j<layer->width; j++) {
					char id = al_get_single_tile_id(layer, j, i);

					if (id == 0) {
						continue;
					}

					ALLEGRO_MAP_TILE *tile = al_get_tile_for_id(map, id);
					if (!tile) {
						// wasn't defined in the map file, presumably because it had no properties
						tile = MALLOC(ALLEGRO_MAP_TILE);
						tile->id = id;
						tile->properties = NULL;
						tile->tileset = NULL;
						tile->bitmap = NULL;

						// locate its tilemap
						SList *tilesets = map->tilesets;
						ALLEGRO_MAP_TILESET *tileset_ref;
						while (tilesets) {
							ALLEGRO_MAP_TILESET *tileset = (ALLEGRO_MAP_TILESET*)tilesets->data;
							tilesets = slist_next(tilesets);
							if (tileset->firstgid <= id) {
								if (!tile->tileset || tileset->firstgid > tile->tileset->firstgid) {
									tileset_ref = tileset;
								}
							}

						}

						tile->tileset = tileset_ref;
						tileset_ref->tiles = slist_prepend(tileset_ref->tiles, tile);
						rb_tree_insert(map->tiles, intdup(tile->id), tile);
					}

					// create this tile's bitmap if it hasn't been yet
					if (!tile->bitmap) {
						ALLEGRO_MAP_TILESET *tileset = tile->tileset;
						int id = tile->id - tileset->firstgid;
						int width = tileset->width / tileset->tilewidth;
						int x = (id % width) * tileset->tilewidth;
						int y = (id / width) * tileset->tileheight;
						tile->bitmap = al_create_sub_bitmap(
								tileset->bitmap,
								x, y,
								tileset->tilewidth,
								tileset->tileheight);
					}
				}
			}
			map->tile_layer_count++;
			map->tile_layers = slist_prepend(map->tile_layers, layer);
		} else if (streq((const char*)layer_node->name, "objectgroup")) {
			layer->type = OBJECT_LAYER;
			layer->objects = NULL;
			layer->object_count = 0;
			// TODO: color?
			SList *objects = get_children_for_name(layer_node, "object");
			SList *object_item = objects;
			while (object_item) {
				xmlNode *object_node = (xmlNode*)object_item->data;
				object_item = slist_next(object_item);

				ALLEGRO_MAP_OBJECT *object = MALLOC(ALLEGRO_MAP_OBJECT);
				object->layer = layer;
				object->name = n_strdup(get_xml_attribute(object_node, "name"));
				object->type = n_strdup(get_xml_attribute(object_node, "type"));
				object->x = atoi(get_xml_attribute(object_node, "x"));
				object->y = atoi(get_xml_attribute(object_node, "y"));

				char *object_width = get_xml_attribute(object_node, "width");
				object->width = (object_width ? atoi(object_width) : 0);

				char *object_height = get_xml_attribute(object_node, "height");
				object->height = (object_height ? atoi(object_height) : 0);

				char *gid = get_xml_attribute(object_node, "gid");
				if (gid) {
					object->gid = atoi(gid);
				}

				char *object_visible = get_xml_attribute(object_node, "visible");
				object->visible = (object_visible ? atoi(object_visible) : 1);

				// Get the object's properties
				object->properties = parse_properties(object_node);
				layer->objects = slist_prepend(layer->objects, object);
				layer->object_count++;
			}
			slist_free(objects);
			map->object_layer_count++;
			map->object_layers = slist_prepend(map->object_layers, layer);
		} else {
			fprintf(stderr, "Error: found invalid layer node \"%s\"\n", layer_node->name);
			continue;
		}

		map->layers = slist_prepend(map->layers, layer);
	}

	slist_free(layers);
	
	// If any objects have a tile gid, cache their image
	layer_item = map->layers;
	while (layer_item) {
		ALLEGRO_MAP_LAYER *layer = (ALLEGRO_MAP_LAYER*)layer_item->data;
		layer_item = slist_next(layer_item);
		if (layer->type != OBJECT_LAYER) {
			continue;
		}

		SList *objects = layer->objects;
		while (objects) {
			ALLEGRO_MAP_OBJECT *object = (ALLEGRO_MAP_OBJECT*)objects->data;
			objects = slist_next(objects);
			if (!object->gid) {
				continue;
			}

			object->bitmap = al_get_tile_for_id(map, object->gid)->bitmap;
			object->width = map->tile_width;
			object->height = map->tile_height;
		}
	}

	xmlFreeDoc(doc);
	al_change_directory(al_path_cstr(cwd, ALLEGRO_NATIVE_PATH_SEP));
	al_destroy_path(cwd);
	return map;
}
