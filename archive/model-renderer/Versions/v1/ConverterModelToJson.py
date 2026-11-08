import json
import re

# Fonction pour lire le fichier MTL et récupérer les textures associées aux matériaux
def parse_mtl(mtl_file):
    textures = {}
    current_material = None

    with open(mtl_file, 'r') as file:
        for line in file:
            if line.startswith('newmtl '):
                current_material = line.split()[1].strip()
            elif line.startswith('map_Kd') and current_material:
                texture_file = line.split()[1].strip()
                textures[current_material] = texture_file
    
    return textures

# Fonction pour lire le fichier OBJ et créer une structure JSON simplifiée
def obj_to_json(obj_file, mtl_textures, output_json):
    vertices = []
    uvs = []
    faces = []
    current_material = None

    with open(obj_file, 'r') as file:
        for line in file:
            if line.startswith('usemtl '):
                current_material = line.split()[1].strip()

            elif line.startswith('v '):
                parts = line.split()
                vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])

            elif line.startswith('vt '):
                parts = line.split()
                uvs.append([float(parts[1]), float(parts[2])])

            elif line.startswith('f '):
                vertex_data = re.findall(r'(\d+)/(\d+)?/?(\d+)?', line)
                coords = []
                uv_coords = []

                for v in vertex_data:
                    vertex_index = int(v[0]) - 1
                    uv_index = int(v[1]) - 1 if v[1] else None

                    coords.append(vertices[vertex_index])
                    if uv_index is not None:
                        uv_coords.append(uvs[uv_index])

                face_type = 'quad' if len(coords) == 4 else 'tris'

                face = {
                    "Type": face_type,
                    "Coord": coords,
                    "UV": uv_coords,
                    "Texture": mtl_textures.get(current_material, "")
                }

                faces.append(face)

    with open(output_json, 'w') as file:
        json.dump(faces, file, indent=4)


# Exemple d'utilisation
if __name__ == "__main__":
    mtl_textures = parse_mtl('Model/Sera.mtl')
    obj_to_json('Model/Sera.obj', mtl_textures, 'Model/Model.json')