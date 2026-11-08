
import re
import struct
import math

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

def normalize(vec):
    x, y, z = vec
    length = math.sqrt(x*x + y*y + z*z)
    if length == 0:
        return [0.0, 0.0, 0.0]
    return [x/length, y/length, z/length]

def average_normals(normals_list):
    x = sum(n[0] for n in normals_list)
    y = sum(n[1] for n in normals_list)
    z = sum(n[2] for n in normals_list)
    return normalize([x, y, z])


class Face:
    def __init__(self, len_vert, coords_idx, uv_idx, normal_idx, texture, face_normal):
        self.len_vert = len_vert
        self.coords = coords_idx
        self.uv = uv_idx
        self.normal_idx = normal_idx
        self.texture = texture
        self.face_normal = face_normal  # [x, y, z]

    def serialize(self, fp):
        fp.write(struct.pack('>B', self.len_vert))
        for i in range(self.len_vert):
            fp.write(struct.pack('>I', self.coords[i]))
            fp.write(struct.pack('>I', self.uv[i]))
        fp.write(struct.pack('>fff', *self.face_normal))
        #fp.write(struct.pack('>H', len(self.texture)))


# Fonction pour lire le fichier OBJ et créer une structure BIN simplifiée
def obj_to_bin(obj_file, mtl_textures, output_bin):
    vertices = []
    uvs = []
    faces = []
    normals = []
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
                uvs.append([float(parts[1]), 1.0 - float(parts[2])])

            elif line.startswith('vn '):
                parts = line.split()
                normals.append([float(parts[1]), float(parts[2]), float(parts[3])])

            elif line.startswith('f '):
                vertex_data = re.findall(r'(\d+)/(\d*)/?(\d*)', line)
                coords_idx = []
                uv_coords_idx = []
                normal_idx = []

                for v in vertex_data:
                    vertex_index = int(v[0]) - 1
                    uv_index = int(v[1]) - 1 if v[1] else 0
                    norm_index = int(v[2]) - 1 if v[2] else 0

                    coords_idx.append(vertex_index)
                    uv_coords_idx.append(uv_index)
                    normal_idx.append(norm_index)

                # Récupère les normales des coins de la face
                face_normals = [normals[i] for i in normal_idx]
                avg_normal = average_normals(face_normals)

                face = Face(len(coords_idx), coords_idx, uv_coords_idx, normal_idx, mtl_textures.get(current_material, ""), avg_normal)
                faces.append(face)

    with open(output_bin, 'wb') as output:
        output.write(struct.pack('>I', len(vertices)))
        output.write(struct.pack('>I', len(uvs)))
        output.write(struct.pack('>I', len(faces)))
        for vertex in vertices:
            #output.write(struct.pack('>fff', vertex[0], vertex[1], vertex[2]))
            output.write(struct.pack('>f', vertex[0]))
            output.write(struct.pack('>f', vertex[1]))
            output.write(struct.pack('>f', vertex[2]))
        for uv in uvs:
            output.write(struct.pack('>f', uv[0]))
            output.write(struct.pack('>f', uv[1]))
        for face in faces:
            face.serialize(output)
    print("Faces: ", len(faces))
    print("Vertices: ", len(vertices))
    print("UVs: ", len(uvs))
    print('Conversion terminée')

# Exemple d'utilisation
if __name__ == "__main__":
    mtl_textures = parse_mtl('Model/Model_low.mtl')
    obj_to_bin('Model/Model_low.obj', mtl_textures, 'Model.bin')