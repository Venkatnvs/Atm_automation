import firebase_admin
from firebase_admin import credentials, db
import cloudinary
import os

DEFAULT_CLOUDINARY_FOLDER = "atm"

def initialize_cloudinary():
    cloudinary.config(
        cloud_name=os.getenv("CLOUDINARY_CLOUD_NAME"),
        api_key=os.getenv("CLOUDINARY_API_KEY"),
        api_secret=os.getenv("CLOUDINARY_API_SECRET")
    )


# Firebase setup
cred = credentials.Certificate("atm-54854-firebase-adminsdk-fbsvc-846f36282a.json")

def initialize_firebase():
    firebase_admin.initialize_app(cred, {"databaseURL": "https://atm-54854-default-rtdb.asia-southeast1.firebasedatabase.app/"})
    return db
