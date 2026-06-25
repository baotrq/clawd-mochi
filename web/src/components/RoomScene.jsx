import { useState } from 'react'
import roomPoster from '../assets/poster.jpg'

// Vite serves files from /public at the app base. Drop your claude.fm-style
// recording in web/public/ as room.webm and/or room.mp4 and it plays here.
const base = import.meta.env.BASE_URL

/**
 * RoomScene — fullscreen looping background video (the animated room).
 *
 * The video is the asset itself, so the scene matches the source exactly with
 * no in-browser reconstruction. While the video loads (or if it's missing),
 * the static room.png poster shows instead.
 */
export default function RoomScene() {
  const [failed, setFailed] = useState(false)

  return (
    <div className="absolute inset-0 bg-room-bg-deep">
      {!failed ? (
        <video
          className="h-full w-full object-cover"
          autoPlay
          loop
          muted
          playsInline
          poster={roomPoster}
          onError={() => setFailed(true)}
        >
          <source src={`${base}room.mp4`} type="video/mp4" />
        </video>
      ) : (
        // No video found yet — show the still frame so the page still looks right.
        <img
          src={roomPoster}
          alt=""
          className="h-full w-full object-cover opacity-90"
        />
      )}
    </div>
  )
}
