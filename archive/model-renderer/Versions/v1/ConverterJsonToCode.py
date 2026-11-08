import json

class Face:
    def __init__(self, face_type, coords, uv_coords, texture):
        self.Type = face_type
        self.Coord = coords
        self.UV = uv_coords
        self.Texture = texture

class Model:
    def __init__(self, json_data):
        self.Faces = []
        for face in json_data:
            self.Faces.append(Face(face["Type"], face["Coord"], face["UV"], face["Texture"]))

    def export(self, file_name):
        src = "#include \"Model.hpp\"\n\n"
        src += "Mesh* load_model()\n"
        src += "{\n"
        
        src += "}\n"
        with open(file_name, "w") as file:
            file.write(src)
if __name__ == "__main__":
    # Load the JSON file
    with open("Model/Model.json", "r") as file:
        data = json.load(file)
    # Create a model object
    model = Model(data)
    # Export the model
    model.export("src/Model.cpp")