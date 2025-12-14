package main

import (
	"fmt"
	"io"
	"log"
	"net"

	pb "aegis.cloud/proto/telemetry" // Generated from telemetry.proto
	"google.golang.org/grpc"
	"google.golang.org/grpc/peer"
)

const (
	port = ":50051"
)

// server implements the gRPC IngestorService
type ingestorServer struct {
	pb.UnimplementedIngestorServiceServer
}

// StreamTelemetry is the main handler for bi-directional streaming
func (s *ingestorServer) StreamTelemetry(stream pb.IngestorService_StreamTelemetryServer) error {
	// Get client IP for logging
	p, _ := peer.FromContext(stream.Context())
	clientIP := p.Addr.String()
	log.Printf("INFO: New stream from %s", clientIP)

	for {
		// 1. Receive data from the Aegis Core unit
		packet, err := stream.Recv()
		if err == io.EOF {
			log.Printf("INFO: Stream from %s closed.", clientIP)
			return nil // Client closed the stream
		}
		if err != nil {
			log.Printf("ERROR: Failed to receive from %s: %v", clientIP, err)
			return err
		}

		// 2. Process the received packet (The "Business Logic")
		// In a real system, this writes to a database or message queue.
		// For this MVP, we just log it to the console.
		handlePacket(packet)

		// 3. (Optional) Send a command back to the Core unit
		// For example, send an ACK for every 10th packet
		// if packet.GetDetection() != nil {
		// 	 stream.Send(&pb.ServerCommand{Command: pb.ServerCommand_ACK})
		// }
	}
}

// handlePacket processes the different types of telemetry payloads
func handlePacket(packet *pb.TelemetryPacket) {
	unitID := packet.GetUnitId()
	ts := packet.GetTimestamp().AsTime()

	switch p := packet.Payload.(type) {
	case *pb.TelemetryPacket_Health:
		health := p.Health
		log.Printf("HEALTH from [%s] at %s: CPU Temp: %.1f°C, GPU Temp: %.1f°C",
			unitID, ts.Format("15:04:05"), health.GetCpuTemperature(), health.GetGpuTemperature())

		// TODO: Write to TimescaleDB

	case *pb.TelemetryPacket_Detection:
		det := p.Detection
		log.Printf("DETECTION from [%s]: TrackID %d, Class %s, Pos(%.1f, %.1f, %.1f)",
			unitID, det.GetTrackId(), det.GetClassId(), det.GetXM(), det.GetYM(), det.GetZM())

		// TODO: Write to PostgreSQL/PostGIS for map visualization

	case *pb.TelemetryPacket_Engagement:
		eng := p.Engagement
		log.Printf("🔥 ENGAGEMENT from [%s]: Target %d, Result %s",
			unitID, eng.GetTargetTrackId(), eng.GetResult())

		// TODO: Trigger a forensics job to analyze the `sensor_log_uri`

	default:
		log.Printf("WARN: Received unknown payload type from %s", unitID)
	}
}

func main() {
	log.Println("--- AEGIS CLOUD: INGESTOR v1.0 ---")

	// Start TCP Listener
	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("FATAL: Failed to listen on port %s: %v", port, err)
	}
	log.Printf("Listening for gRPC connections on %s...", port)

	// Create new gRPC server
	s := grpc.NewServer()

	// Register our service implementation
	pb.RegisterIngestorServiceServer(s, &ingestorServer{})

	// Start serving
	if err := s.Serve(lis); err != nil {
		log.Fatalf("FATAL: gRPC Server failed: %v", err)
	}
}