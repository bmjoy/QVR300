using UnityEngine;
using System.Collections;
using System.Collections.Generic;

public class Recenter : MonoBehaviour 
{
    public GameObject SvrManager;

	public float Damping = .5f;
	public float ExpDampCoef = -20;

	private Transform headTransform;
	private Transform[] objectTransforms;
	private List<Vector3> objectPositions;

	private bool recenter = false;

	void Awake()
	{
		headTransform = SvrManager.GetComponent<SvrManager>().head.transform;
		objectTransforms = gameObject.GetComponentsInChildren<Transform>();
		objectPositions = new List<Vector3>(objectTransforms.Length);
		for(int i=0; i<objectTransforms.Length; i++)
		{
			objectPositions.Add(objectTransforms[i].localPosition);
		}
	}

	void Start () 
    {
	}

	void Update()
	{
		recenter = Input.GetMouseButton(0);
		if (recenter)
		{
			SvrPlugin.Instance.RecenterTracking();
		}
	}

	void LateUpdate()
	{
		for(int i=0; i<objectTransforms.Length; i++)
		{
			Vector3 targetPosition = headTransform.TransformPoint(objectPositions[i]);
			Quaternion targetRotation = headTransform.rotation;
			if (!recenter && Damping > 0)
			{
				objectTransforms[i].position = Vector3.Lerp(objectTransforms[i].position, targetPosition, Damping * (1f - Mathf.Exp(ExpDampCoef * Time.deltaTime)));
				objectTransforms[i].rotation = Quaternion.Slerp(objectTransforms[i].rotation, targetRotation, Damping * (1f - Mathf.Exp(ExpDampCoef * Time.deltaTime)));
			}
			else
			{
				objectTransforms[i].position = targetPosition;
				objectTransforms[i].rotation = targetRotation;
			}
		}
	}

}
