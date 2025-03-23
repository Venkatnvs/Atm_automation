import os
import cv2
import dlib
import pickle
import numpy as np
import requests
from config.setup import db

detector = dlib.get_frontal_face_detector()
predictor = dlib.shape_predictor("models/shape_predictor_68_face_landmarks.dat")
face_rec_model = dlib.face_recognition_model_v1("models/dlib_face_recognition_resnet_model_v1.dat")

encodings_file = "models/encodings.pkl"

if not os.path.exists(encodings_file):
    with open(encodings_file, "wb") as f:
        pickle.dump({"encodings": [], "uuids": []}, f)

with open(encodings_file, "rb") as f:
    data = pickle.load(f)

known_encodings = data["encodings"]
known_uuids = data["uuids"]

print(f"Loaded {len(known_encodings)} encodings.")

# Tolerance for recognition (lower value = more strict matching)
tolerance = 0.6

def remove_user_encodings(user_id):
    global known_encodings, known_uuids
    indices = [i for i, uuid in enumerate(known_uuids) if uuid == user_id]
    for i in reversed(indices):
        known_encodings.pop(i)
        known_uuids.pop(i)
    with open(encodings_file, "wb") as f:
        pickle.dump({"encodings": known_encodings, "uuids": known_uuids}, f)
    print(f"Removed encodings for user {user_id}")

# Recognize face
def recognize_face(face_encoding):
    if len(known_encodings) == 0:
        return "Unknown"

    distances = np.linalg.norm(known_encodings - face_encoding, axis=1)
    min_distance = np.min(distances)
    
    # Calculate confidence score (1 - distance)
    confidence = 1 - min_distance
    
    # Only return a match if confidence is high enough
    if min_distance < tolerance and confidence > 0.4:
        index = np.argmin(distances)
        print(f"Match found with confidence: {confidence:.2f}")
        return known_uuids[index]
    else:
        print(f"No match found. Best confidence: {confidence:.2f}")
    return "Unknown"

# Add new face
def add_new_face(USER_ID):
    global known_encodings, known_uuids

    ref = db.reference(f"users/{USER_ID}")
    data = ref.get()

    if isinstance(data, dict) and not data.get("isAddedToMLModel", True):
        uuid = data.get("uuid", "")
        face_url = data.get("face_url", "")

        print(f"Adding {uuid} to the database., {face_url}")

        if not uuid or not face_url:
            print("Error: Invalid data.")
            return

        response = requests.get(face_url, stream=True)
        if response.status_code != 200:
            raise ValueError(f"Failed to fetch the image from URL: {face_url}")
        
        image_array = np.asarray(bytearray(response.content), dtype=np.uint8)
        frame = cv2.imdecode(image_array, cv2.IMREAD_COLOR)

        if frame is None:
            print("Error: Image not found.")
            return

        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        faces = detector(rgb_frame)

        for face in faces:
            shape = predictor(rgb_frame, face)
            face_encoding = np.array(face_rec_model.compute_face_descriptor(rgb_frame, shape))

            known_encodings.append(face_encoding)
            known_uuids.append(uuid)

            # Save updated encodings
            with open(encodings_file, "wb") as f:
                pickle.dump({"encodings": known_encodings, "uuids": known_uuids}, f)
            print(f"Added {uuid} to the database.")

            # Update the database
            ref.update({"isAddedToMLModel": True})
            print(f"Updated {uuid} in the database.")

# Process frames
def process_image(frame):
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    faces = detector(rgb_frame)
    uuid = None
    for face in faces:
        shape = predictor(rgb_frame, face)
        face_encoding = np.array(face_rec_model.compute_face_descriptor(rgb_frame, shape))
        uuid = recognize_face(face_encoding)
        print(f"Recognized: {uuid}")
    return uuid