#include "quantize.h"
#include <climits>

namespace Quantize {

int *calc_squares() {
    int *rval = new int[MAX_RGB + MAX_RGB + 1];
    for (int i= -MAX_RGB; i <= MAX_RGB; i++) {
        rval[i + MAX_RGB] = i * i;
    }
    return rval;
}
int *SQUARES = calc_squares();

int calc_distance(Pixel &color1, Pixel &color2) {
    return (SQUARES[color2.color.r - color1.color.r + MAX_RGB] +
            SQUARES[color2.color.g - color1.color.g + MAX_RGB] +
            SQUARES[color2.color.b - color1.color.b + MAX_RGB]);
}

void strip_color565(Pixel &color) {
    // Retain last 5 bits of red
    color.color.r &= 0xF8;
    // Retain last 6 bits of green
    color.color.g &= 0xFC;
    // Retain last 5 bits of blue
    color.color.b &= 0xF8;
}

/*
 * =====================================================================
 * ================== CUBE CLASS IMPLEMENTATION ========================
 * =====================================================================
 */

Cube::Cube() {
    pixels = 0;
    colormap = 0;
    root = 0;
    width = 0;
    height = 0;
    max_colors = 0;
    colors = 0;
}

Cube::~Cube() {
    erase();
}

void Cube::erase() {
    if (pixels) {
        for (int x = 0; x < width; x++) {
            delete[] pixels[x];
        }
        delete[] pixels;
        pixels = 0;
    }
    if (colormap) {
        delete[] colormap;
        colormap = 0;
    }
    if (root) {
        delete root;
        root = 0;
    }
}

void Cube::load(QImage &image, int max_colors, bool convertColor565) {
    // Load pixels and set up for colormap data
    loadPixels(image, max_colors, convertColor565);
    this->trueColor = false;
    int x, y;

    // Prior to doing things, pre-calculate a colormap
    // This checks if quantification is needed at all
    // This prevents needless color losses
    // Only do this if the amount of colors is less than 256
    // This to prevent massive memory usage
    if (max_colors <= 256) {
        int colors_tmp = 0;
        Pixel *colormap_tmp = new Pixel[max_colors];
        int i;
        for (x = 0; x < width; x++) {
            for (y = 0; y < height; y++) {
                // Attempt to find the index
                for (i = 0; i < colors_tmp; i++) {
                    if (colormap_tmp[i].value == pixels[x][y].value) {
                        break;
                    }
                }
                // If not found; add it
                if (i == colors_tmp) {
                    colors_tmp++;
                    if (colors_tmp > max_colors) {
                        goto end_colorfind;
                    }
                    colormap_tmp[i].value = pixels[x][y].value;
                }
            }
        }
end_colorfind:
        if (colors_tmp > max_colors) {
            delete[] colormap_tmp;
        } else {
            this->colormap = colormap_tmp;
            this->colors = colors_tmp;
            uint c;
            for (x = 0; x < width; x++) {
                for (y = 0; y < height; y++) {
                    // Find the color at this pixel
                    c = pixels[x][y].value;
                    for (i = 0; i < colors_tmp; i++) {
                        if (c == colormap_tmp[i].value) {
                            pixels[x][y].value = i;
                            break;
                        }
                    }
                }
            }
        }
    }

    // If a colormap is set already, skip all this
    if (!colormap) {
        int i = max_colors;
        // tree_depth = log max_colors
        //                 4
        for (depth = 1; i != 0; depth++) {
            i /= 4;
        }
        if (depth > 1) {
            --depth;
        }
        if (depth > MAX_TREE_DEPTH) {
            depth = MAX_TREE_DEPTH;
        } else if (depth < 2) {
            depth = 2;
        }

        // Define the root node of the octree
        root = new Node(this);

        // Perform the quantization process here
        classification();
        reduction();
        assignment();
    }

    // Apply to the output image
    output = QImage(width, height, QImage::Format_ARGB32);
    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
            Pixel p = colormap[pixels[x][y].value];
            output.setPixel(x, y, p.value);
        }
    }
}

void Cube::loadTrue(QImage &image, bool convertColor565) {
    loadPixels(image, convertColor565 ? 0x10000 : 0x1000000, convertColor565);
    this->colors = this->max_colors;

    // Apply to the output image
    output = QImage(width, height, QImage::Format_ARGB32);
    int x, y;
    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
            output.setPixel(x, y, pixels[x][y].value);
        }
    }
}

void Cube::loadPixels(QImage &image, int max_colors, bool convertColor565) {
    // Erase any previous data - if it exists
    erase();

    // Start filling with data from the image
    this->width = image.width();
    this->height = image.height();
    this->pixels = new Pixel*[width];
    for (int i = 0; i < width; i++) {
        this->pixels[i] = new Pixel[height];
    }
    this->max_colors = max_colors;
    this->colormap = 0;
    this->nodes = 0;
    this->colors = 0;
    this->trueColor = true;

    // Fill pixel data
    int x, y;
    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
            this->pixels[x][y].value = (uint) image.pixel(x, y);
        }
    }

    // Strip 565 color if conversion is needed
    if (convertColor565) {
        for (x = 0; x < width; x++) {
            for (y = 0; y < height; y++) {
                strip_color565(this->pixels[x][y]);
            }
        }
    }
}

Pixel Cube::pixel(int x, int y) {
    if (trueColor) {
        return pixels[x][y];
    } else {
        return colormap[pixels[x][y].value];
    }
}

void Cube::classification() {
    // convert to indexed color
    for (int x = width; --x >= 0; ) {
        for (int y = height; --y >= 0; ) {
            Pixel pixel = pixels[x][y];
            unsigned char red   = pixel.color.r;
            unsigned char green = pixel.color.g;
            unsigned char blue  = pixel.color.b;

            // a hard limit on the number of nodes in the tree
            if (nodes > MAX_NODES) {
                root->pruneLevel();
                --depth;
            }

            // walk the tree to depth, increasing the
            // number_pixels count for each node
            Node *node = root;
            for (int level = 1; level <= depth; ++level) {
                int id = (((red   > node->mid_red   ? 1 : 0) << 0) |
                          ((green > node->mid_green ? 1 : 0) << 1) |
                          ((blue  > node->mid_blue  ? 1 : 0) << 2));
                if (node->child[id] == 0) {
                    node->child[id] = new Node(node, id, level);
                    node->nchild++;
                }
                node = node->child[id];
                node->number_pixels += 1 << (15 - level);
            }

            node->unique++;
            node->total_red   += red;
            node->total_green += green;
            node->total_blue  += blue;
        }
    }
}

void Cube::reduction() {
    int threshold = 1;

    while (colors > max_colors) {
        colors = 0;
        threshold = root->reduce(threshold, INT_MAX);
    }
}

void Cube::assignment() {
    colormap = new Pixel[colors];
    colors = 0;
    root->colormap();

    Search search;

    // convert to indexed color
    for (int x = width; --x >= 0; ) {
        for (int y = height; --y >= 0; ) {
            Pixel pixel = pixels[x][y];
            unsigned char red   = pixel.color.r;
            unsigned char green = pixel.color.g;
            unsigned char blue  = pixel.color.b;

            // walk the tree to find the cube containing that color
            Node *node = root;
            for (;;) {
                int id = (((red   > node->mid_red   ? 1 : 0) << 0) |
                          ((green > node->mid_green ? 1 : 0) << 1) |
                          ((blue  > node->mid_blue  ? 1 : 0) << 2)  );

                if (!node->child[id]) {
                    break;
                }
                node = node->child[id];
            }

            // Find the closest color.
            search.distance = INT_MAX;
            node->parent->closestColor(pixel, &search);
            pixels[x][y].value = search.color_number;
        }
    }
}

/*
 * =====================================================================
 * ================== NODE CLASS IMPLEMENTATION ========================
 * =====================================================================
 */

Node::Node(Cube *cube) {
    init();
    this->cube = cube;
    this->parent = this;
    this->number_pixels = INT_MAX;
    this->mid_red   = (MAX_RGB + 1) >> 1;
    this->mid_green = (MAX_RGB + 1) >> 1;
    this->mid_blue  = (MAX_RGB + 1) >> 1;
}

Node::Node(Node *parent, int id, int level) {
    init();
    this->cube = parent->cube;
    this->parent = parent;
    this->id = id;
    this->level = level;

    // add to the cube
    ++cube->nodes;
    if (level == cube->depth) {
        ++cube->colors;
    }

    // figure out our midpoint
    int bi = (1 << (MAX_TREE_DEPTH - level)) >> 1;
    mid_red   = parent->mid_red   + ((id & 1) > 0 ? bi : -bi);
    mid_green = parent->mid_green + ((id & 2) > 0 ? bi : -bi);
    mid_blue  = parent->mid_blue  + ((id & 4) > 0 ? bi : -bi);
}

Node::~Node() {
    this->cube = 0;
    this->parent = 0;
    while (nchild > 0) {
        delete child[--nchild];
    }
    delete[] child;
}

void Node::init() {
    cube = 0;
    parent = 0;
    nchild = 0;
    child = new Node*[8];
    memset(child, 0, sizeof(Node*) * 8);
    id = 0;
    level = 0;
    number_pixels = 0;
    unique = 0;
    color_number = 0;
    total_red = 0;
    total_green = 0;
    total_blue = 0;
}

void Node::pruneChild() {
    parent->nchild--;
    parent->unique += unique;
    parent->total_red     += total_red;
    parent->total_green   += total_green;
    parent->total_blue    += total_blue;
    parent->child[id] = 0;
    cube->nodes--;
    cube = 0;
    parent = 0;
}

void Node::pruneLevel() {
    if (nchild != 0) {
        for (int id = 0; id < 8; id++) {
            if (child[id]) {
                child[id]->pruneLevel();
            }
        }
    }
    if (level == cube->depth) {
        pruneChild();
    }
}

int Node::reduce(int threshold, int next_threshold) {
    if (nchild != 0) {
        for (int id = 0; id < 8; id++) {
            if (child[id]) {
                next_threshold = child[id]->reduce(threshold, next_threshold);
            }
        }
    }
    if (number_pixels <= threshold) {
        pruneChild();
    } else {
        if (unique != 0) {
            cube->colors++;
        }
        if (number_pixels < next_threshold) {
            next_threshold = number_pixels;
        }
    }
    return next_threshold;
}

void Node::colormap() {
    if (nchild != 0) {
        for (int id = 0; id < 8; id++) {
            if (child[id]) {
                child[id]->colormap();
            }
        }
    }
    if (unique != 0) {
        Pixel p;
        p.color.a = 0xFF;
        p.color.r = ((total_red   + (unique >> 1)) / unique);
        p.color.g = ((total_green + (unique >> 1)) / unique);
        p.color.b = ((total_blue  + (unique >> 1)) / unique);
        cube->colormap[cube->colors] = p;

        color_number = cube->colors++;
    }
}

void Node::closestColor(Pixel &pixel, Search *search) {
    if (nchild != 0) {
        for (int id = 0; id < 8; id++) {
            if (child[id]) {
                child[id]->closestColor(pixel, search);
            }
        }
    }

    if (unique != 0) {
        int distance = calc_distance(cube->colormap[color_number], pixel);
        if (distance < search->distance) {
            search->distance = distance;
            search->color_number = color_number;
        }
    }
}

}